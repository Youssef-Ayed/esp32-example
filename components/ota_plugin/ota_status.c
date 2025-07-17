#include "ota_status.h"
#include "ota_config.h"
#include "ota_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota_status";

static TaskHandle_t heartbeat_task_handle = NULL;
static bool heartbeat_running = false;
static int64_t plugin_start_time = 0;

// Metrics collection
typedef struct
{
    char name[32];
    float value;
    char unit[16];
} ota_metric_t;

static ota_metric_t custom_metrics[10];
static int custom_metrics_count = 0;

static esp_err_t get_device_ip(char *ip_str, size_t ip_str_size)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL)
    {
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret != ESP_OK)
    {
        return ret;
    }

    snprintf(ip_str, ip_str_size, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

static float get_fake_battery_percentage(void)
{
    return 20.0f + (rand() % 81);
}

static int get_wifi_signal_strength(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK)
    {
        return ap_info.rssi;
    }
    return -70; // Default value if can't get real signal
}

static float get_free_heap_percentage(void)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();

    size_t total_heap = free_heap + (free_heap - min_free_heap);
    if (total_heap == 0)
        total_heap = free_heap;

    return ((float)free_heap / total_heap) * 100.0f;
}

static char *create_metrics_json(void)
{
    cJSON *metrics_array = cJSON_CreateArray();

    // Add battery metric
    cJSON *battery_metric = cJSON_CreateObject();
    cJSON_AddStringToObject(battery_metric, "name", "battery_percentage");
    cJSON_AddNumberToObject(battery_metric, "value", get_fake_battery_percentage());
    cJSON_AddStringToObject(battery_metric, "unit", "%");
    cJSON_AddItemToArray(metrics_array, battery_metric);

    // Add WiFi signal strength
    cJSON *wifi_metric = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_metric, "name", "wifi_signal_strength");
    cJSON_AddNumberToObject(wifi_metric, "value", get_wifi_signal_strength());
    cJSON_AddStringToObject(wifi_metric, "unit", "dBm");
    cJSON_AddItemToArray(metrics_array, wifi_metric);

    // Add free heap
    cJSON *heap_metric = cJSON_CreateObject();
    cJSON_AddStringToObject(heap_metric, "name", "free_heap_percentage");
    cJSON_AddNumberToObject(heap_metric, "value", get_free_heap_percentage());
    cJSON_AddStringToObject(heap_metric, "unit", "%");
    cJSON_AddItemToArray(metrics_array, heap_metric);

    // Add custom metrics
    for (int i = 0; i < custom_metrics_count; i++)
    {
        cJSON *custom_metric = cJSON_CreateObject();
        cJSON_AddStringToObject(custom_metric, "name", custom_metrics[i].name);
        cJSON_AddNumberToObject(custom_metric, "value", custom_metrics[i].value);
        cJSON_AddStringToObject(custom_metric, "unit", custom_metrics[i].unit);
        cJSON_AddItemToArray(metrics_array, custom_metric);
    }

    char *json_string = cJSON_Print(metrics_array);
    cJSON_Delete(metrics_array);

    return json_string;
}

static void heartbeat_task(void *pvParameters)
{
    char ip_str[16];

    ESP_LOGI(TAG, "Heartbeat task started");

    while (heartbeat_running)
    {
        if (get_device_ip(ip_str, sizeof(ip_str)) == ESP_OK)
        {
            char *metrics_json = NULL;

            if (OTA_METRICS_ENABLED)
            {
                metrics_json = create_metrics_json();
            }

            uint32_t uptime_sec = (esp_timer_get_time() - plugin_start_time) / 1000000;

            esp_err_t err = ota_http_send_heartbeat(DEVICE_ID, uptime_sec, ip_str,
                                                    FIRMWARE_REF, metrics_json);

            if (err == ESP_OK)
            {
                ESP_LOGD(TAG, "Heartbeat sent successfully");
            }
            else
            {
                ESP_LOGW(TAG, "Failed to send heartbeat: %s", esp_err_to_name(err));
            }

            if (metrics_json)
            {
                free(metrics_json);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to get device IP for heartbeat");
        }

        vTaskDelay(pdMS_TO_TICKS(OTA_HEARTBEAT_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "Heartbeat task stopped");
    heartbeat_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t ota_status_init(void)
{
    plugin_start_time = esp_timer_get_time();
    custom_metrics_count = 0;
    ESP_LOGI(TAG, "Status module initialized");
    return ESP_OK;
}

esp_err_t ota_status_start_heartbeat(void)
{
    if (heartbeat_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    heartbeat_running = true;

    BaseType_t ret = xTaskCreate(heartbeat_task, "heartbeat_task",
                                 OTA_HEARTBEAT_TASK_STACK_SIZE, NULL,
                                 OTA_HEARTBEAT_TASK_PRIORITY, &heartbeat_task_handle);

    if (ret != pdPASS)
    {
        heartbeat_running = false;
        ESP_LOGE(TAG, "Failed to create heartbeat task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Heartbeat started");
    return ESP_OK;
}

esp_err_t ota_status_stop_heartbeat(void)
{
    if (!heartbeat_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    heartbeat_running = false;

    // Wait for task to finish
    if (heartbeat_task_handle != NULL)
    {
        while (heartbeat_task_handle != NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_LOGI(TAG, "Heartbeat stopped");
    return ESP_OK;
}

esp_err_t ota_status_add_custom_metric(const char *name, float value, const char *unit)
{
    if (!name || !unit || custom_metrics_count >= 10)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(custom_metrics[custom_metrics_count].name, name, sizeof(custom_metrics[0].name) - 1);
    custom_metrics[custom_metrics_count].name[sizeof(custom_metrics[0].name) - 1] = '\0';

    custom_metrics[custom_metrics_count].value = value;

    strncpy(custom_metrics[custom_metrics_count].unit, unit, sizeof(custom_metrics[0].unit) - 1);
    custom_metrics[custom_metrics_count].unit[sizeof(custom_metrics[0].unit) - 1] = '\0';

    custom_metrics_count++;

    ESP_LOGD(TAG, "Added custom metric: %s = %.2f %s", name, value, unit);
    return ESP_OK;
}

esp_err_t ota_status_clear_custom_metrics(void)
{
    custom_metrics_count = 0;
    ESP_LOGD(TAG, "Cleared custom metrics");
    return ESP_OK;
}

uint32_t ota_status_get_uptime_sec(void)
{
    return (esp_timer_get_time() - plugin_start_time) / 1000000;
}

bool ota_status_is_heartbeat_running(void)
{
    return heartbeat_running;
}
