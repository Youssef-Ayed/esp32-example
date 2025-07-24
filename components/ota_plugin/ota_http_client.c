#include "ota_http_client.h"
#include "ota_config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "ota_http_client";

// HTTP response buffer
typedef struct
{
    char *buffer;
    int buffer_len;
    int data_len;
} http_response_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buffer_t *output_buffer = (http_response_buffer_t *)evt->user_data;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (output_buffer != NULL && evt->data_len > 0)
        {
            // Reallocate buffer if needed
            if (output_buffer->data_len + evt->data_len >= output_buffer->buffer_len)
            {
                output_buffer->buffer_len = output_buffer->data_len + evt->data_len + 1;
                output_buffer->buffer = realloc(output_buffer->buffer, output_buffer->buffer_len);
                if (output_buffer->buffer == NULL)
                {
                    ESP_LOGE(TAG, "Failed to reallocate memory for response buffer");
                    return ESP_FAIL;
                }
            }

            memcpy(output_buffer->buffer + output_buffer->data_len, evt->data, evt->data_len);
            output_buffer->data_len += evt->data_len;
            output_buffer->buffer[output_buffer->data_len] = '\0';
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t ota_http_client_init(void)
{
    ESP_LOGI(TAG, "HTTP client initialized");
    return ESP_OK;
}

esp_err_t ota_http_post_json(const char *endpoint, const char *json_data, char *response_buffer, size_t response_buffer_size)
{
    if (!endpoint || !json_data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char url[OTA_URL_BUFFER_SIZE];
    snprintf(url, sizeof(url), "%s%s", OTA_SERVER_BASE_URL, endpoint);

    http_response_buffer_t output_buffer = {
        .buffer = malloc(OTA_MAX_HTTP_OUTPUT_BUFFER),
        .buffer_len = OTA_MAX_HTTP_OUTPUT_BUFFER,
        .data_len = 0};

    if (output_buffer.buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &output_buffer,
        .timeout_ms = OTA_SERVER_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = !OTA_SSL_VERIFICATION,
        .keep_alive_enable = true,
        .keep_alive_idle = 5,
        .keep_alive_interval = 5,
        .keep_alive_count = 3,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(output_buffer.buffer);
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-OTA-Plugin/1.0");

    // Set POST data
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    // Perform request
    err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 status_code, esp_http_client_get_content_length(client));

        if (status_code >= 200 && status_code < 300)
        {
            if (response_buffer && response_buffer_size > 0 && output_buffer.data_len > 0)
            {
                size_t copy_len = (output_buffer.data_len < response_buffer_size - 1) ? output_buffer.data_len : response_buffer_size - 1;
                memcpy(response_buffer, output_buffer.buffer, copy_len);
                response_buffer[copy_len] = '\0';
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP request failed with status %d", status_code);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(output_buffer.buffer);
    return err;
}

esp_err_t ota_http_check_firmware_update(const char *device_id, const char *current_version,
                                         bool *update_available, char *firmware_url, size_t url_size,
                                         char *new_version, size_t version_size)
{
    if (!device_id || !current_version || !update_available)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON request
    cJSON *json = cJSON_CreateObject();
    cJSON *device_id_json = cJSON_CreateString(device_id);
    cJSON *version_json = cJSON_CreateString(current_version);

    cJSON_AddItemToObject(json, "deviceId", device_id_json);
    cJSON_AddItemToObject(json, "version", version_json);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string)
    {
        ESP_LOGE(TAG, "Failed to create JSON request");
        return ESP_ERR_NO_MEM;
    }

    // Make HTTP request
    char response[OTA_JSON_BUFFER_SIZE];
    esp_err_t err = ota_http_post_json("/firmware/check", json_string, response, sizeof(response));
    free(json_string);

    if (err != ESP_OK)
    {
        return err;
    }

    // Parse response
    cJSON *response_json = cJSON_Parse(response);
    if (!response_json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *update_available_json = cJSON_GetObjectItem(response_json, "updateAvailable");
    if (cJSON_IsBool(update_available_json))
    {
        *update_available = cJSON_IsTrue(update_available_json);

        if (*update_available)
        {
            cJSON *firmware_url_json = cJSON_GetObjectItem(response_json, "firmwareUrl");
            cJSON *new_version_json = cJSON_GetObjectItem(response_json, "version");

            if (firmware_url && url_size > 0 && cJSON_IsString(firmware_url_json))
            {
                strncpy(firmware_url, firmware_url_json->valuestring, url_size - 1);
                firmware_url[url_size - 1] = '\0';
            }

            if (new_version && version_size > 0 && cJSON_IsString(new_version_json))
            {
                strncpy(new_version, new_version_json->valuestring, version_size - 1);
                new_version[version_size - 1] = '\0';
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "Invalid response format");
        err = ESP_FAIL;
    }

    cJSON_Delete(response_json);
    return err;
}

esp_err_t ota_http_report_firmware_status(const char *device_id, const char *version, const char *status)
{
    if (!device_id || !version || !status)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON request
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "deviceId", device_id);
    cJSON_AddStringToObject(json, "version", version);
    cJSON_AddStringToObject(json, "status", status);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string)
    {
        ESP_LOGE(TAG, "Failed to create JSON request");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ota_http_post_json("/firmware/report", json_string, NULL, 0);
    free(json_string);

    return err;
}

esp_err_t ota_http_send_heartbeat(const char *device_id, uint32_t uptime_sec, const char *ip,
                                  const char *firmware_ref, const char *metrics_json)
{
    if (!device_id || !ip || !firmware_ref)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON request
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "deviceId", device_id);
    cJSON_AddNumberToObject(json, "uptimeSec", uptime_sec);
    cJSON_AddStringToObject(json, "ip", ip);
    cJSON_AddStringToObject(json, "firmwareRef", firmware_ref);

    // Add metrics array
    if (metrics_json)
    {
        cJSON *metrics = cJSON_Parse(metrics_json);
        if (metrics)
        {
            cJSON_AddItemToObject(json, "metrics", metrics);
        }
    }
    else
    {
        cJSON_AddItemToObject(json, "metrics", cJSON_CreateArray());
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string)
    {
        ESP_LOGE(TAG, "Failed to create JSON request");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ota_http_post_json("/heartbeat", json_string, NULL, 0);
    free(json_string);

    return err;
}

esp_err_t ota_http_send_log(const char *device_id, const char *level, const char *message,
                            const char *stack_trace, const char *context)
{
    if (!device_id || !level || !message)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON request
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "deviceId", device_id);
    cJSON_AddStringToObject(json, "level", level);
    cJSON_AddStringToObject(json, "message", message);

    if (stack_trace)
    {
        cJSON_AddStringToObject(json, "stack_trace", stack_trace);
    }

    if (context)
    {
        cJSON_AddStringToObject(json, "context", context);
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string)
    {
        ESP_LOGE(TAG, "Failed to create JSON request");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ota_http_post_json("/log", json_string, NULL, 0);
    free(json_string);

    return err;
}

esp_err_t ota_http_send_trace(const char *device_id, const char *trace_id, const char *span_id,
                              const char *parent_span_id, const char *operation, uint32_t duration_ms,
                              int64_t started_at, int64_t ended_at, const char *attributes)
{
    if (!device_id || !trace_id || !span_id || !operation)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON request
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "deviceId", device_id);
    cJSON_AddStringToObject(json, "trace_id", trace_id);
    cJSON_AddStringToObject(json, "span_id", span_id);
    cJSON_AddStringToObject(json, "operation", operation);
    cJSON_AddNumberToObject(json, "duration_ms", duration_ms);
    cJSON_AddNumberToObject(json, "started_at", (double)started_at);
    cJSON_AddNumberToObject(json, "ended_at", (double)ended_at);

    if (parent_span_id && strlen(parent_span_id) > 0)
    {
        cJSON_AddStringToObject(json, "parent_span", parent_span_id);
    }

    if (attributes)
    {
        cJSON *attrs = cJSON_Parse(attributes);
        if (attrs)
        {
            cJSON_AddItemToObject(json, "attributes", attrs);
        }
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string)
    {
        ESP_LOGE(TAG, "Failed to create JSON request");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ota_http_post_json("/trace", json_string, NULL, 0);
    free(json_string);

    return err;
}

esp_err_t ota_http_download_and_install_firmware(const char *firmware_url)
{
    if (!firmware_url)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA update from: %s", firmware_url);

    esp_http_client_config_t http_config = {
        .url = firmware_url,
        .timeout_ms = OTA_SERVER_TIMEOUT_MS,
        .use_global_ca_store = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = !OTA_SSL_VERIFICATION,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA update successful, restarting...");
        esp_restart();
    }
    else
    {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }

    return ret;
}