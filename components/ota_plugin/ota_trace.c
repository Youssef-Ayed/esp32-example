#include "ota_trace.h"
#include "ota_config.h"
#include "ota_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "ota_trace";

static uint32_t trace_counter = 0;
static uint32_t span_counter = 0;

static void generate_trace_id(char* trace_id, size_t size) {
    snprintf(trace_id, size, "trace_%08lx_%08lx", (unsigned long)esp_timer_get_time(), ++trace_counter);
}

static void generate_span_id(char* span_id, size_t size) {
    snprintf(span_id, size, "span_%08lx", ++span_counter);
}

esp_err_t ota_trace_init(void) {
    trace_counter = 0;
    span_counter = 0;
    ESP_LOGI(TAG, "Trace module initialized");
    return ESP_OK;
}

ota_trace_context_t* ota_trace_start_operation(const char* operation, const char* parent_span_id) {
    if (!OTA_TRACING_ENABLED || !operation) {
        return NULL;
    }
    
    ota_trace_context_t* ctx = malloc(sizeof(ota_trace_context_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate memory for trace context");
        return NULL;
    }
    
    memset(ctx, 0, sizeof(ota_trace_context_t));
    
    generate_trace_id(ctx->trace_id, sizeof(ctx->trace_id));
    generate_span_id(ctx->span_id, sizeof(ctx->span_id));
    
    if (parent_span_id) {
        strncpy(ctx->parent_span_id, parent_span_id, sizeof(ctx->parent_span_id) - 1);
        ctx->parent_span_id[sizeof(ctx->parent_span_id) - 1] = '\0';
    }
    
    strncpy(ctx->operation, operation, sizeof(ctx->operation) - 1);
    ctx->operation[sizeof(ctx->operation) - 1] = '\0';
    
    ctx->start_time = esp_timer_get_time();
    
    ESP_LOGD(TAG, "Started trace: %s [%s]", operation, ctx->trace_id);
    return ctx;
}

esp_err_t ota_trace_end_operation(ota_trace_context_t* trace_ctx, const char* attributes) {
    if (!OTA_TRACING_ENABLED || !trace_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int64_t end_time = esp_timer_get_time();
    uint32_t duration_ms = (end_time - trace_ctx->start_time) / 1000;
    
    esp_err_t err = ota_http_send_trace(DEVICE_ID, trace_ctx->trace_id, trace_ctx->span_id,
                                       strlen(trace_ctx->parent_span_id) > 0 ? trace_ctx->parent_span_id : NULL,
                                       trace_ctx->operation, duration_ms,
                                       trace_ctx->start_time, end_time, attributes);
    
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Trace sent: %s completed in %lu ms", trace_ctx->operation, (unsigned long)duration_ms);
    } else {
        ESP_LOGW(TAG, "Failed to send trace: %s", esp_err_to_name(err));
    }
    
    free(trace_ctx);
    return err;
}

esp_err_t ota_trace_add_event(ota_trace_context_t* trace_ctx, const char* event_name, const char* attributes) {
    if (!OTA_TRACING_ENABLED || !trace_ctx || !event_name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // For simplicity, we'll send events as separate spans with very short duration
    char event_span_id[OTA_SPAN_ID_SIZE];
    generate_span_id(event_span_id, sizeof(event_span_id));
    
    int64_t event_time = esp_timer_get_time();
    
    esp_err_t err = ota_http_send_trace(DEVICE_ID, trace_ctx->trace_id, event_span_id,
                                       trace_ctx->span_id, event_name, 0,
                                       event_time, event_time, attributes);
    
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Trace event sent: %s", event_name);
    } else {
        ESP_LOGW(TAG, "Failed to send trace event: %s", esp_err_to_name(err));
    }
    
    return err;
}

const char* ota_trace_get_trace_id(ota_trace_context_t* trace_ctx) {
    return trace_ctx ? trace_ctx->trace_id : NULL;
}

const char* ota_trace_get_span_id(ota_trace_context_t* trace_ctx) {
    return trace_ctx ? trace_ctx->span_id : NULL;
}
