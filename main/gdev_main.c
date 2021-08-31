/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"

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

void print_msg(example_espnow_event_recv_cb_t *recv_cb)
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

    //* Don't delete this, keep to print data if we need to
    // for (uint32_t i = 0; i < data_len; i++)
    // {
    //     printf("payload[%d]: %d\n", i, payload[i]);
    // }
    // printf("data_len: %d\n", data_len);
    //* ---------------------------------------------------

    for (uint32_t i = 0; i < data_len; i++)
    {
        printf("@%d.", payload[i]);
    }
    printf("\n");

    free(payload);

    if (data_len < sizeof(example_espnow_data_t))
    {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
    }
}

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

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    example_wifi_init();
    example_espnow_init();
}
