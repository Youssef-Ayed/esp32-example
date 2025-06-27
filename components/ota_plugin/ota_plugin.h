#ifndef OTA_PLUGIN_H
#define OTA_PLUGIN_H

#include "ota_config.h"
#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for trace context
typedef struct ota_trace_context_s ota_trace_context_t;

/**
 * @brief Initialize the OTA plugin
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_plugin_init(void);

/**
 * @brief Start OTA background tasks
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_plugin_start(void);

/**
 * @brief Stop OTA background tasks
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_plugin_stop(void);

/**
 * @brief Deinitialize the OTA plugin
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_plugin_deinit(void);

/**
 * @brief Manually trigger OTA update check
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_plugin_check_update(void);

/**
 * @brief Log a message to the remote server
 * @param level Log level
 * @param message Log message
 * @param stack_trace Optional stack trace
 * @param context Optional context
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_log(ota_log_level_t level, const char* message, const char* stack_trace, const char* context);

/**
 * @brief Start a trace operation
 * @param operation Operation name
 * @param parent_span_id Optional parent span ID (can be NULL)
 * @return Trace context pointer on success, NULL on failure
 */
ota_trace_context_t* ota_trace_start(const char* operation, const char* parent_span_id);

/**
 * @brief End a trace operation
 * @param trace_ctx Trace context from ota_trace_start
 * @param attributes Optional JSON attributes string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_trace_end(ota_trace_context_t* trace_ctx, const char* attributes);

/**
 * @brief Get current plugin status
 * @return Current OTA status
 */
ota_status_t ota_plugin_get_status(void);

/**
 * @brief Get device uptime in seconds
 * @return Uptime in seconds
 */
uint32_t ota_plugin_get_uptime_sec(void);

/**
 * @brief Send custom metric
 * @param name Metric name
 * @param value Metric value
 * @param unit Metric unit
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_plugin_send_metric(const char* name, float value, const char* unit);

// Trace context structure
struct ota_trace_context_s {
    char trace_id[OTA_TRACE_ID_SIZE];
    char span_id[OTA_SPAN_ID_SIZE];
    char parent_span_id[OTA_SPAN_ID_SIZE];
    char operation[64];
    int64_t start_time;
};

#ifdef __cplusplus
}
#endif

#endif // OTA_PLUGIN_H