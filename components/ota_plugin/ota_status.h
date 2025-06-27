#ifndef OTA_STATUS_H
#define OTA_STATUS_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize status module
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_status_init(void);

/**
 * @brief Start heartbeat task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_status_start_heartbeat(void);

/**
 * @brief Stop heartbeat task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_status_stop_heartbeat(void);

/**
 * @brief Add custom metric to be included in heartbeat
 * @param name Metric name
 * @param value Metric value
 * @param unit Metric unit
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_status_add_custom_metric(const char* name, float value, const char* unit);

/**
 * @brief Clear all custom metrics
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_status_clear_custom_metrics(void);

/**
 * @brief Get device uptime in seconds
 * @return Uptime in seconds
 */
uint32_t ota_status_get_uptime_sec(void);

/**
 * @brief Check if heartbeat is running
 * @return true if running, false otherwise
 */
bool ota_status_is_heartbeat_running(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_STATUS_H
