#ifndef OTA_HTTP_CLIENT_H
#define OTA_HTTP_CLIENT_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize HTTP client
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_client_init(void);

/**
 * @brief Send HTTP POST request with JSON data
 * @param endpoint API endpoint (relative to base URL)
 * @param json_data JSON payload
 * @param response_buffer Buffer to store response (can be NULL)
 * @param response_buffer_size Size of response buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_post_json(const char* endpoint, const char* json_data, 
                           char* response_buffer, size_t response_buffer_size);

/**
 * @brief Check for firmware updates
 * @param device_id Device identifier
 * @param current_version Current firmware version
 * @param update_available Output: true if update is available
 * @param firmware_url Output: URL of new firmware
 * @param url_size Size of firmware_url buffer
 * @param new_version Output: new firmware version
 * @param version_size Size of new_version buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_check_firmware_update(const char* device_id, const char* current_version, 
                                       bool* update_available, char* firmware_url, size_t url_size, 
                                       char* new_version, size_t version_size);

/**
 * @brief Report firmware update status
 * @param device_id Device identifier
 * @param version Firmware version
 * @param status Update status ("COMPLETED" or "FAILED")
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_report_firmware_status(const char* device_id, const char* version, const char* status);

/**
 * @brief Send heartbeat with metrics
 * @param device_id Device identifier
 * @param uptime_sec Device uptime in seconds
 * @param ip Device IP address
 * @param firmware_ref Current firmware reference
 * @param metrics_json JSON array of metrics (can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_send_heartbeat(const char* device_id, uint32_t uptime_sec, const char* ip, 
                                 const char* firmware_ref, const char* metrics_json);

/**
 * @brief Send log message
 * @param device_id Device identifier
 * @param level Log level ("info", "warn", "error", "fatal")
 * @param message Log message
 * @param stack_trace Optional stack trace
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_send_log(const char* device_id, const char* level, const char* message, 
                           const char* stack_trace, const char* context);

/**
 * @brief Send trace data
 * @param device_id Device identifier
 * @param trace_id Trace identifier
 * @param span_id Span identifier
 * @param parent_span_id Parent span identifier (can be NULL)
 * @param operation Operation name
 * @param duration_ms Operation duration in milliseconds
 * @param started_at Start timestamp
 * @param ended_at End timestamp
 * @param attributes Optional JSON attributes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_send_trace(const char* device_id, const char* trace_id, const char* span_id, 
                             const char* parent_span_id, const char* operation, uint32_t duration_ms, 
                             int64_t started_at, int64_t ended_at, const char* attributes);

/**
 * @brief Download and install firmware
 * @param firmware_url URL of firmware to download
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_http_download_and_install_firmware(const char* firmware_url);

#ifdef __cplusplus
}
#endif

#endif // OTA_HTTP_CLIENT_H
