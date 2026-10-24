# Smart Trash 🗑️

Sistema IoT para monitoramento do nível de preenchimento de lixeiras, utilizando ESP32, sensor ultrassônico, comunicação MQTT e um painel web.

## Sobre o projeto

O Smart Trash é um protótipo acadêmico desenvolvido com o objetivo de auxiliar no monitoramento de lixeiras e fornecer informações que possam contribuir para o planejamento da coleta de resíduos.

O sistema utiliza um sensor ultrassônico conectado a um ESP32 para medir a distância até a superfície dos resíduos. Os dados são transmitidos por Wi-Fi utilizando o protocolo MQTT e apresentados em um dashboard web, que calcula uma estimativa do nível de preenchimento da lixeira.

A arquitetura permite monitorar múltiplas lixeiras por meio de um único broker MQTT.

## Funcionalidades

- Medição de distância utilizando o sensor ultrassônico HC-SR04.
- Conectividade Wi-Fi por meio do ESP32.
- Comunicação entre dispositivos e aplicações utilizando MQTT.
- Descoberta dinâmica de lixeiras a partir das mensagens recebidas.
- Visualização do nível estimado de preenchimento em um dashboard web.
- Arquitetura preparada para receber dados de múltiplas lixeiras.

## Arquitetura do sistema

```text
Sensor ultrassônico HC-SR04
             |
             v
       Firmware ESP32
             |
             | Wi-Fi / MQTT
             v
       Broker Mosquitto
             |
             | MQTT sobre WebSockets
             v
       Dashboard web
```

### Componentes

| Componente | Responsabilidade |
|---|---|
| ESP32 | Realiza as leituras do sensor e publica os dados. |
| HC-SR04 | Mede a distância até a superfície dos resíduos. |
| MQTT | Define a comunicação por troca de mensagens entre os clientes. |
| Mosquitto | Atua como broker, recebendo e distribuindo mensagens MQTT. |
| Dashboard web | Apresenta as medições e estima o nível de preenchimento. |

## Estrutura do repositório

```text
smart_trash_project/
├── smart_trash/           # Firmware ESP32 com ESP-IDF
└── smart-trash-dashboard/ # Interface web
```

## Comunicação MQTT

Cada lixeira publica uma mensagem JSON contendo seu identificador, a altura útil configurada e a distância medida.

Exemplo de tópico:

```text
trash/CAN67
```

Exemplo de mensagem:

```json
{
  "trash_name": "CAN67",
  "trash_size": 28,
  "distance_cm": 14
}
```

Os campos representam:

- `trash_name`: identificador da lixeira.
- `trash_size`: altura útil configurada, em centímetros.
- `distance_cm`: distância corrigida entre o sensor e a superfície dos resíduos, em centímetros.

O dashboard se inscreve no filtro `trash/#`, recebendo mensagens dos tópicos pertencentes à hierarquia `trash/`.

Quando recebe uma mensagem válida, a interface identifica a lixeira e cria um cartão para ela caso ainda não exista. As mensagens seguintes atualizam o cartão correspondente.

## Estimativa do nível de preenchimento

A porcentagem estimada é calculada pelo dashboard a partir da altura útil configurada e da distância medida:

**Preenchimento (%) = ((Altura útil − Distância) / Altura útil) × 100**

Para uma lixeira com altura útil de 28 cm:

- Distância de 28 cm: aproximadamente 0% preenchida.
- Distância de 14 cm: aproximadamente 50% preenchida.
- Distância de 0 cm: aproximadamente 100% preenchida.

O cálculo estima o preenchimento vertical da lixeira. Não representa uma medição direta do volume ou da massa dos resíduos, e sua precisão depende da instalação do sensor, da calibração e da distribuição do lixo.

## Como executar

### Requisitos

- Placa de desenvolvimento ESP32.
- Sensor ultrassônico HC-SR04.
- Ambiente de desenvolvimento ESP-IDF.
- Rede Wi-Fi acessível ao ESP32 e ao computador que hospeda o broker.
- Broker MQTT Mosquitto.
- Navegador web moderno.

### 1. Firmware

Entre no diretório `smart_trash/` em um ambiente configurado para o ESP-IDF.

Configure as credenciais locais de Wi-Fi e os parâmetros necessários. Em seguida, compile e grave o firmware:

```bash
idf.py build
idf.py -p PORT flash monitor
```

Substitua `PORT` pelo dispositivo serial correspondente à placa ESP32.

### 2. Broker MQTT

Instale e configure o Mosquitto para aceitar conexões MQTT pela porta TCP `1883`.

O dashboard utiliza MQTT sobre WebSockets, portanto o broker também precisa de um listener WebSocket configurado, por exemplo, na porta `9001`.

O ESP32 e o computador que executa o broker precisam conseguir se comunicar pela rede.

### 3. Dashboard

Entre no diretório `smart-trash-dashboard/` e inicie um servidor HTTP local:

```bash
python -m http.server 8080
```

Acesse o endereço abaixo no navegador:

```text
http://localhost:8080
```

Configure no dashboard o endereço WebSocket do broker e o filtro de inscrição desejado, como `trash/#`.

## Segurança

- Não versionar senhas de Wi-Fi, tokens ou outras credenciais.
- Manter arquivos de configuração local fora do controle de versão.
- Utilizar acesso MQTT anônimo somente em ambientes controlados de desenvolvimento.
- Não expor as portas do broker diretamente à internet sem mecanismos adequados de autenticação e segurança.

## Estado do projeto

O Smart Trash é um protótipo acadêmico. O sistema já realiza medições com o sensor, transmite os dados via MQTT e apresenta as mensagens recebidas no dashboard.

Como possíveis evoluções, estão o armazenamento histórico das medições, a geração de alertas para lixeiras próximas da capacidade máxima e o apoio à otimização das rotas de coleta.

## Licença

Nenhuma licença foi definida até o momento.
