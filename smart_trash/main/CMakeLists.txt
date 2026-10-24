idf_component_register(SRCS "smart_trash.c"
                    REQUIRES hc_sr04 esp_wifi esp_event esp_netif nvs_flash mqtt
                    INCLUDE_DIRS ".")
