#ifndef OTA_LOG_H
#define OTA_LOG_H

#include "ota_config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize log module
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log_init(void);

/**
 * @brief Send log message with specified level
 * @param level Log level
 * @param message Log message
 * @param stack_trace Optional stack trace
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log_send(ota_log_level_t level, const char* message, const char* stack_trace, const char* context);

/**
 * @brief Send info log message
 * @param message Log message
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log_info(const char* message, const char* context);

/**
 * @brief Send warning log message
 * @param message Log message
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log_warn(const char* message, const char* context);

/**
 * @brief Send error log message
 * @param message Log message
 * @param stack_trace Optional stack trace
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log_error(const char* message, const char* stack_trace, const char* context);

/**
 * @brief Send fatal log message
 * @param message Log message
 * @param stack_trace Optional stack trace
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log_fatal(const char* message, const char* stack_trace, const char* context);

#ifdef __cplusplus
}
#endif

#endif // OTA_LOG_H
