/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
// #include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_netif.h"
// #include "esp_eth.h"
// #include "esp_event.h"
#include "esp_log.h"
// #include "driver/gpio.h"
// #include "sdkconfig.h"

// mqtt
// #include <stdint.h>
// #include <stddef.h>
// #include "esp_system.h"
#include "nvs_flash.h"
// #include "freertos/semphr.h"
// #include "freertos/queue.h"
// #include "lwip/sockets.h"
// #include "lwip/dns.h"
// #include "lwip/netdb.h"
// #include "mqtt_client.h"
// end mqtt

// espnow
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "espnow_gdev.h"

static xQueueHandle s_example_espnow_queue;
static xQueueHandle gateway_queue;

static void example_espnow_deinit(void);

void print_msg(example_espnow_event_recv_cb_t *recv_cb);

// end espnow

static const char *TAG = "gateway_dev";

// mqtt
// use IP address from "IOTstack_Net" network. Log into portainer (port 9000) and scroll
// to the bottom of Mosquitto container. Choose "IP Address"
// #define CONFIG_BROKER_URL "192.168.1.145"
// #define CONFIG_BROKER_URL "192.168.1.106"

/**
 * Send espnow data over mqtt
 * sends complete raw data (up to 250 bytes)
 * (maybe include other data as well?...)
 */
// int send_espnow_data(esp_mqtt_client_handle_t client, char *data, int data_len)
// {
//     int msg_id = esp_mqtt_client_publish(client, "/topic/espnow_data", data, data_len, 1, 0);
//     // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//     return msg_id;
// }

// int send_data(esp_mqtt_client_handle_t client, int count)
// {
//     char scnt[10];
//     sprintf(scnt, "%d", count);

//     int msg_id = esp_mqtt_client_publish(client, "/topic/qos0", scnt, 0, 1, 0);
//     ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//     return msg_id;
// }

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
// static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
// {
//     ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
//     esp_mqtt_event_handle_t event = event_data;
//     esp_mqtt_client_handle_t client = event->client;
//     int msg_id;
//     switch ((esp_mqtt_event_id_t)event_id)
//     {
//     case MQTT_EVENT_CONNECTED:
//         ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
//         msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
//         ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

//         msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
//         ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

//         msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
//         ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

//         msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
//         ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
//         break;
//     case MQTT_EVENT_DISCONNECTED:
//         ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
//         break;
//     case MQTT_EVENT_SUBSCRIBED:
//         ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
//         msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
//         ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//         break;
//     case MQTT_EVENT_UNSUBSCRIBED:
//         ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
//         break;
//     case MQTT_EVENT_PUBLISHED:
//         ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
//         break;
//     case MQTT_EVENT_DATA:
//         ESP_LOGI(TAG, "MQTT_EVENT_DATA");
//         printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
//         printf("DATA=%.*s\r\n", event->data_len, event->data);
//         break;
//     case MQTT_EVENT_ERROR:
//         ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
//         break;
//     default:
//         ESP_LOGI(TAG, "Other event id:%d", event->event_id);
//         break;
//     }
// }

void print_msg(example_espnow_event_recv_cb_t *recv_cb)
{
    uint16_t data_len = recv_cb->data_len;

    example_espnow_data_t *buf = (example_espnow_data_t *)recv_cb->data;

    uint8_t *payload = (uint8_t *)malloc(data_len + 12); // add 12 bytes for 2x MAC addresses (batt_dev & g_dev)

    //AND add 1 byte for \0 (null char)

    memcpy(payload, buf->payload, data_len);

    // temp
    uint32_t temp = 0;
    for (int i = 0; i < 4; i++)
    {
        temp += payload[i] << (i * 8);
    }
    ESP_LOGI(TAG, "temp: %.4f", (float)(temp / 16.0));

    // program time
    uint32_t ptime_ms = 0;
    for (int i = 4; i < 8; i++)
    {
        ptime_ms += payload[i] << (i * 8);
    }
    ESP_LOGI(TAG, "ptime_ms: %d", ptime_ms);

    // bat voltage
    uint32_t batmv = 0;
    for (int i = 8; i < 12; i++)
    {
        batmv += payload[i] << (i * 8);
    }
    ESP_LOGI(TAG, "batmv: %d", batmv);

    // add batt_dev MAC
    for (int i = 0; i < 6; i++)
    {
        payload[data_len + i] = recv_cb->mac_addr[i];
    }
    data_len += 6;

    unsigned char mac_base[6] = {0};
    esp_efuse_mac_get_default(mac_base);
    esp_read_mac(mac_base, ESP_MAC_ETH);

    ESP_LOGI(TAG, "gateway device mac: " MACSTR, MAC2STR(mac_base));

    // add g_dev MAC (this device)
    for (int j = 0; j < 6; j++)
    {
        payload[data_len + j] = mac_base[j];
    }
    data_len += 6;

    // payload[data_len] = '\0';
    // data_len++;

    //* Don't delete this, keep to print data if we need to
    // for (uint32_t i = 0; i < data_len; i++)
    // {
    //     printf("payload[%d]: %d\n", i, payload[i]);
    // }
    // printf("data_len: %d\n", data_len);
    //* ---------------------------------------------------

    // send data over mqtt:
    // send_espnow_data(client, (char *)payload, data_len);

    // printf("payload:\n");
    // printf("%s\n", payload);
    // printf("\n");

    // printf("@");
    for (uint32_t i = 0; i < data_len; i++)
    {
        printf("@%d.", payload[i]);
    }
    printf("\n");
    // printf("nr_msg%s\n", payload);
    // printf("M%.*s\n", (int)sizeof(payload), payload);
    // printf("done\n");

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    // free(payload);

    if (data_len < sizeof(example_espnow_data_t))
    {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
    }
}

// static void mqtt_app_start(void)
// {
//     esp_mqtt_client_config_t mqtt_cfg = {
//         .host = CONFIG_BROKER_URL,
//         .port = 1883,
//         .transport = MQTT_TRANSPORT_OVER_TCP,
//         .protocol_ver = MQTT_PROTOCOL_V_3_1_1,
//     };

//     esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
//     /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
//     esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
//     esp_mqtt_client_start(client);

//     // if theres data in the queue (from espnow receive), send it over mqtt
//     // gateway_queue

//     example_espnow_event_recv_cb_t *recv_cb;
/*
    while (xQueueReceive(gateway_queue, &recv_cb, portMAX_DELAY) == pdTRUE)
    {
        uint16_t data_len = recv_cb->data_len;

        example_espnow_data_t *buf = (example_espnow_data_t *)recv_cb->data;

        uint8_t *payload = (uint8_t *)malloc(data_len + 12); // add 12 bytes for 2x MAC addresses (batt_dev & g_dev)

        memcpy(payload, buf->payload, data_len);

        // temp
        uint32_t temp = 0;
        for (int i = 0; i < 4; i++)
        {
            temp += payload[i] << (i * 8);
        }
        ESP_LOGI(TAG, "temp: %.4f", (float)(temp / 16.0));

        // program time
        uint32_t ptime_ms = 0;
        for (int i = 4; i < 8; i++)
        {
            ptime_ms += payload[i] << (i * 8);
        }
        ESP_LOGI(TAG, "ptime_ms: %d", ptime_ms);

        // bat voltage
        uint32_t batmv = 0;
        for (int i = 8; i < 12; i++)
        {
            batmv += payload[i] << (i * 8);
        }
        ESP_LOGI(TAG, "batmv: %d", batmv);

        // add batt_dev MAC
        for (int i = 0; i < 6; i++)
        {
            payload[data_len + i] = recv_cb->mac_addr[i];
        }
        data_len += 6;

        unsigned char mac_base[6] = {0};
        esp_efuse_mac_get_default(mac_base);
        esp_read_mac(mac_base, ESP_MAC_ETH);

        ESP_LOGI(TAG, "gateway device mac: " MACSTR, MAC2STR(mac_base));

        // add g_dev MAC (this device)
        for (int j = 0; j < 6; j++)
        {
            payload[data_len + j] = mac_base[j];
        }
        data_len += 6;

        // Don't delete this, keep to print data if we need to
        // for (uint32_t i = 0; i < data_len; i++)
        // {
        //     printf("payload[%d]: %d\n", i, payload[i]);
        // }
        // printf("data_len: %d\n", data_len);
        // ---------------------------------------------------

        // send data over mqtt:
        send_espnow_data(client, (char *)payload, data_len);

        free(payload);

        if (data_len < sizeof(example_espnow_data_t))
        {
            ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        }
    }
    */
// }

// end mqtt

// espnow

/* WiFi should start before using ESPNOW */
static void example_wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
}

static void example_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_example_espnow_queue, &evt, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

/* Parse received ESPNOW data. */
esp_err_t example_espnow_data_parse(uint8_t *data, uint16_t data_len)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

static void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;
    esp_err_t ret;

    while (xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case EXAMPLE_ESPNOW_RECV_CB:
        {
            example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

            ret = example_espnow_data_parse(recv_cb->data, recv_cb->data_len);

            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "============== Received data from: " MACSTR ", len: %d ==============", MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                // add data to queue to be sent over ethernet
                // should probably only add data to queue if it isnt "Error data", but work out why it's saying it's error data in the future...

                print_msg(recv_cb);
                // printf("ok\n");

                // if (xQueueSend(gateway_queue, &recv_cb, portMAX_DELAY) != pdTRUE)
                // {
                //     ESP_LOGW(TAG, "Send gateway queue fail");
                // }
            }
            else if (ret == ESP_FAIL)
            {
                ESP_LOGE(TAG, "ret == ESP_FAIL, must be an error with crc value");
            }
            else
            {
                ESP_LOGE(TAG, "shouldn't get to this...");
            }

            break;
        }
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

static esp_err_t example_espnow_init(void)
{
    s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    gateway_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_recv_cb_t));

    if (s_example_espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail: s_example_espnow_queue");
        return ESP_FAIL;
    }

    if (gateway_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail: gateway_queue");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    xTaskCreate(example_espnow_task, "example_espnow_task", 2048, NULL, 4, NULL);

    return ESP_OK;
}

static void example_espnow_deinit()
{
    vSemaphoreDelete(s_example_espnow_queue);
    esp_now_deinit();
}

// endespnow

// /** Event handler for Ethernet events */
// static void eth_event_handler(void *arg, esp_event_base_t event_base,
//                               int32_t event_id, void *event_data)
// {
//     uint8_t mac_addr[6] = {0};
//     /* we can get the ethernet driver handle from event data */
//     esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

//     switch (event_id)
//     {
//     case ETHERNET_EVENT_CONNECTED:
//         esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
//         ESP_LOGI(TAG, "Ethernet Link Up");
//         ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
//                  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
//         break;
//     case ETHERNET_EVENT_DISCONNECTED:
//         ESP_LOGI(TAG, "Ethernet Link Down");
//         break;
//     case ETHERNET_EVENT_START:
//         ESP_LOGI(TAG, "Ethernet Started");
//         break;
//     case ETHERNET_EVENT_STOP:
//         ESP_LOGI(TAG, "Ethernet Stopped");
//         break;
//     default:
//         break;
//     }
// }

// /** Event handler for IP_EVENT_ETH_GOT_IP */
// static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
//                                  int32_t event_id, void *event_data)
// {
//     ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//     const esp_netif_ip_info_t *ip_info = &event->ip_info;

//     ESP_LOGI(TAG, "Ethernet Got IP Address");
//     ESP_LOGI(TAG, "~~~~~~~~~~~");
//     ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
//     ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
//     ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
//     ESP_LOGI(TAG, "~~~~~~~~~~~");

//     mqtt_app_start();
// }

// #define PIN_PHY_POWER 12
void app_main(void)
{
    /*
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Set default handlers to process TCP/IP stuffs
    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
    gpio_pad_select_gpio(PIN_PHY_POWER);
    gpio_set_direction(PIN_PHY_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_PHY_POWER, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET
    mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_EXAMPLE_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_LAN8720
    esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
#elif CONFIG_EXAMPLE_USE_DM9051
    gpio_install_isr_service(0);
    spi_device_handle_t spi_handle = NULL;
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_DM9051_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_DM9051_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_DM9051_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_DM9051_SPI_HOST, &buscfg, 1));
    spi_device_interface_config_t devcfg = {
        .command_bits = 1,
        .address_bits = 7,
        .mode = 0,
        .clock_speed_hz = CONFIG_EXAMPLE_DM9051_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EXAMPLE_DM9051_CS_GPIO,
        .queue_size = 20};
    ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_EXAMPLE_DM9051_SPI_HOST, &devcfg, &spi_handle));
    */
    /* dm9051 ethernet driver is based on spi driver */
    /*
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(spi_handle);
    dm9051_config.int_gpio_num = CONFIG_EXAMPLE_DM9051_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#endif

    // mqtt
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
*/
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // mqtt end

    // esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    // esp_eth_handle_t eth_handle = NULL;
    // ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    /* attach Ethernet driver to TCP/IP stack */
    // ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    /* start Ethernet driver state machine */
    // ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    example_wifi_init();
    example_espnow_init();
}
