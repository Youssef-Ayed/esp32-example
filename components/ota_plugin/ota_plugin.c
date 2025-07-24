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
#define NVS_KEY_CURRENT_VERSION "current_version"

static char current_firmware_version[32] = OTA_FIRMWARE_VERSION;

static esp_err_t load_current_firmware_version(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Could not open NVS, using default firmware version: %s", OTA_FIRMWARE_VERSION);
        strcpy(current_firmware_version, OTA_FIRMWARE_VERSION);
        return ESP_OK;
    }

    size_t required_size = sizeof(current_firmware_version);
    err = nvs_get_str(nvs_handle, NVS_KEY_CURRENT_VERSION, current_firmware_version, &required_size);
    if (err != ESP_OK)
    {
        // First boot - initialize NVS with default version
        ESP_LOGI(TAG, "First boot detected, initializing firmware version: %s", OTA_FIRMWARE_VERSION);
        strcpy(current_firmware_version, OTA_FIRMWARE_VERSION);

        // Save the default version to NVS for future boots
        err = nvs_set_str(nvs_handle, NVS_KEY_CURRENT_VERSION, current_firmware_version);
        if (err == ESP_OK)
        {
            nvs_commit(nvs_handle);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Loaded firmware version from NVS: %s", current_firmware_version);
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t save_current_firmware_version(const char *version)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_CURRENT_VERSION, version);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK)
        {
            strcpy(current_firmware_version, version);
            ESP_LOGI(TAG, "Updated firmware version to: %s", version);
        }
    }

    nvs_close(nvs_handle);
    return err;
}

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
                                                               strlen(last_version) > 0 ? last_version : current_firmware_version,
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

        esp_err_t err = ota_http_check_firmware_update(DEVICE_ID, current_firmware_version,
                                                       &update_available, firmware_url,
                                                       sizeof(firmware_url), new_version,
                                                       sizeof(new_version));

        if (err == ESP_OK)
        {
            if (update_available)
            {
                ESP_LOGI(TAG, "Firmware update available: %s -> %s", current_firmware_version, new_version);
                ota_log_info("Firmware update available", new_version);

                if (trace_ctx)
                {
                    ota_trace_add_event(trace_ctx, "update_available", NULL);
                }

                current_status = OTA_STATUS_DOWNLOADING;

                // Save new version and status BEFORE attempting update
                // (because esp_https_ota restarts device on success)
                save_current_firmware_version(new_version);
                save_update_status("COMPLETED", new_version);

                ESP_LOGI(TAG, "Starting firmware download and installation...");
                err = ota_http_download_and_install_firmware(firmware_url);

                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "OTA update completed successfully, restarting...");
                    current_status = OTA_STATUS_SUCCESS;
                    // Device will restart automatically from esp_https_ota
                }
                else
                {
                    // Restore old version and set failure status on error
                    save_current_firmware_version(current_firmware_version);
                    save_update_status("FAILED", new_version);

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

    // Initialize NVS FIRST
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

    // NOW load current firmware version from NVS
    load_current_firmware_version();
    ESP_LOGI(TAG, "Using firmware version: %s", current_firmware_version);

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
    ota_log_info("OTA plugin initialized", current_firmware_version);

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

    esp_err_t err = ota_http_check_firmware_update(DEVICE_ID, current_firmware_version,
                                                   &update_available, firmware_url,
                                                   sizeof(firmware_url), new_version,
                                                   sizeof(new_version));

    if (err == ESP_OK)
    {
        if (update_available)
        {
            ESP_LOGI(TAG, "Manual check: Update available %s -> %s", current_firmware_version, new_version);
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