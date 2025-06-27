#ifndef OTA_TRACE_H
#define OTA_TRACE_H

#include "ota_plugin.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize trace module
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_trace_init(void);

/**
 * @brief Start a trace operation
 * @param operation Operation name
 * @param parent_span_id Optional parent span ID
 * @return Trace context on success, NULL on failure
 */
ota_trace_context_t* ota_trace_start_operation(const char* operation, const char* parent_span_id);

/**
 * @brief End a trace operation
 * @param trace_ctx Trace context from ota_trace_start_operation
 * @param attributes Optional JSON attributes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_trace_end_operation(ota_trace_context_t* trace_ctx, const char* attributes);

/**
 * @brief Add an event to an existing trace
 * @param trace_ctx Trace context
 * @param event_name Event name
 * @param attributes Optional JSON attributes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ota_trace_add_event(ota_trace_context_t* trace_ctx, const char* event_name, const char* attributes);

/**
 * @brief Get trace ID from context
 * @param trace_ctx Trace context
 * @return Trace ID string or NULL if invalid context
 */
const char* ota_trace_get_trace_id(ota_trace_context_t* trace_ctx);

/**
 * @brief Get span ID from context
 * @param trace_ctx Trace context
 * @return Span ID string or NULL if invalid context
 */
const char* ota_trace_get_span_id(ota_trace_context_t* trace_ctx);

#ifdef __cplusplus
}
#endif

#endif // OTA_TRACE_H
