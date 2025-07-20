#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ota_plugin.h"

static const char *TAG = "main";

// Example WiFi credentials - replace with your actual credentials
#define EXAMPLE_ESP_WIFI_SSID "JOJO"
#define EXAMPLE_ESP_WIFI_PASS "JOJO240523"
#define EXAMPLE_ESP_MAXIMUM_RETRY 5

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            ESP_LOGI(TAG, "connect to the AP fail");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 OTA Plugin Example");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init_sta();

    // Wait for WiFi connection
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Initialize OTA plugin
    ESP_LOGI(TAG, "Initializing OTA plugin...");
    ret = ota_plugin_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize OTA plugin: %s", esp_err_to_name(ret));
        return;
    }

    // Start OTA plugin
    ESP_LOGI(TAG, "Starting OTA plugin...");
    ret = ota_plugin_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start OTA plugin: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "OTA plugin is now running!");

    // Example: Send some custom metrics periodically
    int counter = 0;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Wait 30 seconds

        // Send custom metric
        ota_plugin_send_metric("loop_counter", counter++, "count");

        // Example: Send a log message every 5 iterations
        if (counter % 5 == 0)
        {
            ota_log(OTA_LOG_LEVEL_INFO, "Application is running normally", NULL, "main_loop");
        }

        // Example: Manual OTA check every 10 iterations
        if (counter % 10 == 0)
        {
            ESP_LOGI(TAG, "Performing manual OTA check...");
            ota_plugin_check_update();
        }

        ESP_LOGI(TAG, "App running, iteration: %d, status: %d", counter, ota_plugin_get_status());
    }
}