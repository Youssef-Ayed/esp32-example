#include "ota_log.h"
#include "ota_config.h"
#include "ota_http_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ota_log";

// Helper function to convert log level to string
static const char* log_level_to_string(ota_log_level_t level) {
    switch (level) {
        case OTA_LOG_LEVEL_INFO:
            return "info";
        case OTA_LOG_LEVEL_WARN:
            return "warn";
        case OTA_LOG_LEVEL_ERROR:
            return "error";
        case OTA_LOG_LEVEL_FATAL:
            return "fatal";
        default:
            return "info";
    }
}

// Initialize the log module
esp_err_t ota_log_init(void) {
    ESP_LOGI(TAG, "Log module initialized");
    return ESP_OK;
}

// Send log message with specified level
esp_err_t ota_log_send(ota_log_level_t level, const char* message, const char* stack_trace, const char* context) {
    if (!OTA_LOGGING_ENABLED) {
        return ESP_OK; // Logging disabled, but not an error
    }
    
    if (!message) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* level_str = log_level_to_string(level);
    
    esp_err_t err = ota_http_send_log(DEVICE_ID, level_str, message, stack_trace, context);
    
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Log sent: [%s] %s", level_str, message);
    } else {
        ESP_LOGW(TAG, "Failed to send log: %s", esp_err_to_name(err));
        // Log locally as fallback
        switch (level) {
            case OTA_LOG_LEVEL_INFO:
                ESP_LOGI("REMOTE_LOG", "%s", message);
                break;
            case OTA_LOG_LEVEL_WARN:
                ESP_LOGW("REMOTE_LOG", "%s", message);
                break;
            case OTA_LOG_LEVEL_ERROR:
            case OTA_LOG_LEVEL_FATAL:
                ESP_LOGE("REMOTE_LOG", "%s", message);
                break;
        }
    }
    
    return err;
}

// Convenience functions for different log levels
esp_err_t ota_log_info(const char* message, const char* context) {
    return ota_log_send(OTA_LOG_LEVEL_INFO, message, NULL, context);
}

esp_err_t ota_log_warn(const char* message, const char* context) {
    return ota_log_send(OTA_LOG_LEVEL_WARN, message, NULL, context);
}

esp_err_t ota_log_error(const char* message, const char* stack_trace, const char* context) {
    return ota_log_send(OTA_LOG_LEVEL_ERROR, message, stack_trace, context);
}

esp_err_t ota_log_fatal(const char* message, const char* stack_trace, const char* context) {
    return ota_log_send(OTA_LOG_LEVEL_FATAL, message, stack_trace, context);
}
