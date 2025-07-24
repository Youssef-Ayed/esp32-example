#include "ota_plugin.h"
#include "ota_config.h"
#include "ota_http_client.h"
#include "ota_status.h"
#include "ota_log.h"
#include "ota_trace.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ota_plugin";

static bool plugin_initialized = false;
static bool plugin_running = false;
static ota_status_t current_status = OTA_STATUS_IDLE;
static TaskHandle_t ota_task_handle = NULL;
static int64_t plugin_start_time = 0;

// NVS keys
#define NVS_NAMESPACE "ota_plugin"
#define NVS_KEY_UPDATE_STATUS "update_status"
#define NVS_KEY_LAST_VERSION "last_version"

static esp_err_t check_and_report_boot_status(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Check if we have an update status to report
    char update_status[16] = {0};
    size_t required_size = sizeof(update_status);
    err = nvs_get_str(nvs_handle, NVS_KEY_UPDATE_STATUS, update_status, &required_size);

    if (err == ESP_OK && strlen(update_status) > 0)
    {
        // We have a status to report
        char last_version[32] = {0};
        required_size = sizeof(last_version);
        nvs_get_str(nvs_handle, NVS_KEY_LAST_VERSION, last_version, &required_size);

        ESP_LOGI(TAG, "Reporting boot status: %s for version %s", update_status, last_version);

        // Report the status
        esp_err_t report_err = ota_http_report_firmware_status(DEVICE_ID,
                                                               strlen(last_version) > 0 ? last_version : FIRMWARE_REF,
                                                               update_status);

        if (report_err == ESP_OK)
        {
            ESP_LOGI(TAG, "Boot status reported successfully");
            // Clear the status from NVS
            nvs_erase_key(nvs_handle, NVS_KEY_UPDATE_STATUS);
            nvs_erase_key(nvs_handle, NVS_KEY_LAST_VERSION);
            nvs_commit(nvs_handle);
        }
        else
        {
            ESP_LOGW(TAG, "Failed to report boot status: %s", esp_err_to_name(report_err));
        }
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t save_update_status(const char *status, const char *version)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_UPDATE_STATUS, status);
    if (err == ESP_OK && version)
    {
        err = nvs_set_str(nvs_handle, NVS_KEY_LAST_VERSION, version);
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return err;
}

static void ota_check_task(void *pvParameters)
{
    ESP_LOGI(TAG, "OTA check task started");

    while (plugin_running)
    {
        ota_trace_context_t *trace_ctx = ota_trace_start("ota_update_check", NULL);

        current_status = OTA_STATUS_CHECKING;

        bool update_available = false;
        char firmware_url[OTA_URL_BUFFER_SIZE] = {0};
        char new_version[64] = {0};

        esp_err_t err = ota_http_check_firmware_update(DEVICE_ID, OTA_FIRMWARE_VERSION,
                                                       &update_available, firmware_url,
                                                       sizeof(firmware_url), new_version,
                                                       sizeof(new_version));

        if (err == ESP_OK)
        {
            if (update_available)
            {
                ESP_LOGI(TAG, "Firmware update available: %s -> %s", OTA_FIRMWARE_VERSION, new_version);
                ota_log_info("Firmware update available", new_version);

                if (trace_ctx)
                {
                    ota_trace_add_event(trace_ctx, "update_available", NULL);
                }

                current_status = OTA_STATUS_DOWNLOADING;

                // Save status before attempting update
                save_update_status("FAILED", new_version); // Assume failure, update on success

                ESP_LOGI(TAG, "Starting firmware download and installation...");
                err = ota_http_download_and_install_firmware(firmware_url);

                if (err == ESP_OK)
                {
                    // This shouldn't be reached as device should restart
                    ESP_LOGI(TAG, "OTA update completed successfully");
                    current_status = OTA_STATUS_SUCCESS;
                    // Update the firmware version in NVS or other persistent storage
                    save_update_status("COMPLETED", new_version);
                    // Update the macro or variable for OTA_FIRMWARE_VERSION
                    // (This requires a mechanism to reload or redefine the macro dynamically)
                }
                else
                {
                    ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
                    current_status = OTA_STATUS_FAILED;
                    ota_log_error("OTA update failed", esp_err_to_name(err), new_version);

                    if (trace_ctx)
                    {
                        ota_trace_add_event(trace_ctx, "update_failed", NULL);
                    }
                }
            }
            else
            {
                ESP_LOGD(TAG, "No firmware update available");
                current_status = OTA_STATUS_IDLE;
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to check for firmware update: %s", esp_err_to_name(err));
            current_status = OTA_STATUS_FAILED;
            ota_log_warn("Failed to check for firmware update", esp_err_to_name(err));
        }

        if (trace_ctx)
        {
            ota_trace_end_operation(trace_ctx, NULL);
        }

        // Wait before next check
        vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "OTA check task stopped");
    ota_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t ota_plugin_init(void)
{
    if (plugin_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing OTA plugin...");
    plugin_start_time = esp_timer_get_time();

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Initialize sub-modules
    err = ota_http_client_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client: %s", esp_err_to_name(err));
        return err;
    }

    err = ota_status_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize status module: %s", esp_err_to_name(err));
        return err;
    }

    err = ota_log_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize log module: %s", esp_err_to_name(err));
        return err;
    }

    err = ota_trace_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize trace module: %s", esp_err_to_name(err));
        return err;
    }

    plugin_initialized = true;
    current_status = OTA_STATUS_IDLE;

    ESP_LOGI(TAG, "OTA plugin initialized successfully");
    ota_log_info("OTA plugin initialized", FIRMWARE_REF);

    return ESP_OK;
}

esp_err_t ota_plugin_start(void)
{
    if (!plugin_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (plugin_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting OTA plugin...");

    // Check and report boot status first
    check_and_report_boot_status();

    // Start heartbeat
    esp_err_t err = ota_status_start_heartbeat();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start heartbeat: %s", esp_err_to_name(err));
        return err;
    }

    // Start OTA check task
    plugin_running = true;
    BaseType_t ret = xTaskCreate(ota_check_task, "ota_check_task",
                                 OTA_TASK_STACK_SIZE, NULL,
                                 OTA_TASK_PRIORITY, &ota_task_handle);

    if (ret != pdPASS)
    {
        plugin_running = false;
        ota_status_stop_heartbeat();
        ESP_LOGE(TAG, "Failed to create OTA check task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA plugin started successfully");
    ota_log_info("OTA plugin started", NULL);

    return ESP_OK;
}

esp_err_t ota_plugin_stop(void)
{
    if (!plugin_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping OTA plugin...");

    plugin_running = false;

    // Stop heartbeat
    ota_status_stop_heartbeat();

    // Wait for OTA task to finish
    if (ota_task_handle != NULL)
    {
        while (ota_task_handle != NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    current_status = OTA_STATUS_IDLE;

    ESP_LOGI(TAG, "OTA plugin stopped");
    ota_log_info("OTA plugin stopped", NULL);

    return ESP_OK;
}

esp_err_t ota_plugin_deinit(void)
{
    if (plugin_running)
    {
        esp_err_t err = ota_plugin_stop();
        if (err != ESP_OK)
        {
            return err;
        }
    }

    if (!plugin_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    plugin_initialized = false;
    current_status = OTA_STATUS_IDLE;

    ESP_LOGI(TAG, "OTA plugin deinitialized");
    return ESP_OK;
}

esp_err_t ota_plugin_check_update(void)
{
    if (!plugin_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Manual OTA update check requested");

    ota_trace_context_t *trace_ctx = ota_trace_start("manual_ota_check", NULL);

    bool update_available = false;
    char firmware_url[OTA_URL_BUFFER_SIZE] = {0};
    char new_version[64] = {0};

    esp_err_t err = ota_http_check_firmware_update(DEVICE_ID, OTA_FIRMWARE_VERSION,
                                                   &update_available, firmware_url,
                                                   sizeof(firmware_url), new_version,
                                                   sizeof(new_version));

    if (err == ESP_OK)
    {
        if (update_available)
        {
            ESP_LOGI(TAG, "Manual check: Update available %s -> %s", OTA_FIRMWARE_VERSION, new_version);
            ota_log_info("Manual OTA check: Update available", new_version);
        }
        else
        {
            ESP_LOGI(TAG, "Manual check: No update available");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Manual check failed: %s", esp_err_to_name(err));
        ota_log_warn("Manual OTA check failed", esp_err_to_name(err));
    }

    if (trace_ctx)
    {
        ota_trace_end_operation(trace_ctx, NULL);
    }

    return err;
}

esp_err_t ota_log(ota_log_level_t level, const char *message, const char *stack_trace, const char *context)
{
    return ota_log_send(level, message, stack_trace, context);
}

ota_trace_context_t *ota_trace_start(const char *operation, const char *parent_span_id)
{
    return ota_trace_start_operation(operation, parent_span_id);
}

esp_err_t ota_trace_end(ota_trace_context_t *trace_ctx, const char *attributes)
{
    return ota_trace_end_operation(trace_ctx, attributes);
}

ota_status_t ota_plugin_get_status(void)
{
    return current_status;
}

uint32_t ota_plugin_get_uptime_sec(void)
{
    return (esp_timer_get_time() - plugin_start_time) / 1000000;
}

esp_err_t ota_plugin_send_metric(const char *name, float value, const char *unit)
{
    return ota_status_add_custom_metric(name, value, unit);
}