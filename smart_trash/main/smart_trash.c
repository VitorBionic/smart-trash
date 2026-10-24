#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hc_sr04.h"

#define WIFI_SSID       "O Secreto"
#define WIFI_PASSWORD   "vitor6969"

#define TRASH_NAME      "CAN67"
#define TRASH_SIZE      28

#define TRIGGER_GPIO    GPIO_NUM_19
#define ECHO_GPIO       GPIO_NUM_18

#define SAMPLE_COUNT    3
#define SAMPLE_DELAY_MS 200
#define REPORT_INTERVAL_MS 30000
#define SENSOR_OFFSET_CM 2.0f

#define MQTT_BROKER_URI "mqtt://172.19.122.125:1883"
#define MQTT_TOPIC      "trash/" TRASH_NAME

static const char *TAG = "SMART_TRASH";
static const char *MQTT_TAG = "MQTT";

static hc_sr04_handle_t sensor = NULL;

static esp_mqtt_client_handle_t mqtt_client = NULL;
static volatile bool mqtt_connected = false;


/* ==================== Wi-Fi ==================== */

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {

        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {

        ESP_LOGW(TAG,
                 "Wi-Fi desconectado; tentando reconectar");

        mqtt_connected = false;
        esp_wifi_connect();

    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Wi-Fi conectado! IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};

    strlcpy((char *)wifi_config.sta.ssid,
            WIFI_SSID, sizeof(wifi_config.sta.ssid));

    strlcpy((char *)wifi_config.sta.password,
            WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_set_config(
        WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
}


/* ==================== Sensor ==================== */

static void sensor_init(void)
{
    hc_sr04_config_t config = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO,
        .timeout_us = 30000,
        .sound_speed_cm_per_us = 0.0f
    };

    ESP_ERROR_CHECK(hc_sr04_new_sensor(&config, &sensor));
}

static bool read_average_distance(float *distance_cm)
{
    float sum = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float reading = 0.0f;

        esp_err_t ret = hc_sr04_read_cm(sensor, &reading);

        if (ret == ESP_OK &&
            isfinite(reading) &&
            reading > 0.0f) {

            sum += reading;
            valid_samples++;

            ESP_LOGI(TAG, "Leitura %d: %.2f cm",
                     i + 1, reading);

        } else {
            ESP_LOGW(TAG, "Falha na leitura %d: %s",
                     i + 1, esp_err_to_name(ret));
        }

        if (i < SAMPLE_COUNT - 1) {
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
        }
    }

    if (valid_samples == 0) {
        ESP_LOGE(TAG, "Nenhuma leitura valida");
        return false;
    }

    float average = sum / valid_samples;

    *distance_cm = roundf(
        fmaxf(0.0f, average - SENSOR_OFFSET_CM));

    ESP_LOGI(TAG,
             "%s | tamanho: %d cm | media: %.2f cm | "
             "corrigida: %.2f cm (%d/%d leituras)",
             TRASH_NAME, TRASH_SIZE, average,
             *distance_cm, valid_samples, SAMPLE_COUNT);

    return true;
}


/* ==================== MQTT ==================== */

static void mqtt_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(MQTT_TAG, "Conectado ao broker MQTT");
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(MQTT_TAG, "Desconectado do broker MQTT");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(MQTT_TAG, "Erro no cliente MQTT");
        break;

    default:
        break;
    }
}

static void mqtt_init(void)
{
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&config);

    ESP_ERROR_CHECK(
        mqtt_client != NULL ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL));

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void mqtt_publish_distance(float distance_cm)
{
    if (!mqtt_connected) {
        ESP_LOGW(MQTT_TAG,
                 "Sem conexao MQTT; mensagem nao enviada");
        return;
    }

    char payload[128];

    snprintf(payload, sizeof(payload),
             "{\"trash_name\":\"%s\","
             "\"trash_size\":%d,"
             "\"distance_cm\":%.0f}",
             TRASH_NAME, TRASH_SIZE, distance_cm);

    int message_id = esp_mqtt_client_publish(
        mqtt_client,
        MQTT_TOPIC,
        payload,
        0,
        1,
        0);

    if (message_id >= 0) {
        ESP_LOGI(MQTT_TAG,
                 "Publicacao solicitada: %s", payload);
    } else {
        ESP_LOGE(MQTT_TAG,
                 "Falha ao publicar mensagem");
    }
}


/* ==================== Aplicacao ==================== */

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    sensor_init();
    wifi_init();
    mqtt_init();
    vTaskDelay(pdMS_TO_TICKS(200)); // espera um tempinho se não a primeira leitura do sensor sai errado

    while (1) {
        float distance_cm;

        if (read_average_distance(&distance_cm)) {
            mqtt_publish_distance(distance_cm);
        }

        vTaskDelay(pdMS_TO_TICKS(REPORT_INTERVAL_MS));
    }
}
