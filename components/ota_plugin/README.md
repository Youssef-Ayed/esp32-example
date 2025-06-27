# ESP32 OTA + Telemetry Plugin

A comprehensive modular OTA (Over-The-Air) and telemetry plugin for ESP32 projects using ESP-IDF. This plugin provides automatic firmware updates, heartbeat monitoring, remote logging, and distributed tracing capabilities.

## Features

- **Automatic OTA Updates**: Periodically checks for firmware updates and installs them automatically
- **Heartbeat & Metrics**: Sends device status, uptime, and custom metrics to a remote server
- **Remote Logging**: Sends application logs to a remote server for centralized monitoring
- **Distributed Tracing**: Tracks operation performance with distributed tracing capabilities
- **Modular Design**: Each feature is implemented in separate modules for maintainability
- **Configurable**: Easy configuration through header file
- **Robust Error Handling**: Comprehensive error handling and retry mechanisms

## Architecture

The plugin consists of several modules:

- `ota_plugin.c/h`: Main plugin API and coordination
- `ota_config.h`: Configuration settings
- `ota_http_client.c/h`: HTTP client for API communication
- `ota_status.c/h`: Heartbeat and metrics collection
- `ota_log.c/h`: Remote logging functionality
- `ota_trace.c/h`: Distributed tracing implementation

## Backend Integration

The plugin communicates with the following REST API endpoints:

### 1. Firmware Check

- **Endpoint**: `POST /firmware/check`
- **Body**: `{ deviceId: string, version: string }`
- **Response**: `{ updateAvailable: boolean, firmwareUrl?: string, version?: string }`

### 2. Firmware Report

- **Endpoint**: `POST /firmware/report`
- **Body**: `{ deviceId: string, version: string, status: "COMPLETED" | "FAILED" }`

### 3. Heartbeat

- **Endpoint**: `POST /heartbeat`
- **Body**: `{ deviceId: string, uptimeSec: number, ip: string, firmwareRef: string, metrics: Array<{name, value, unit}> }`

### 4. Logging

- **Endpoint**: `POST /log`
- **Body**: `{ deviceId: string, level: string, message: string, stack_trace?: string, context?: string }`

### 5. Tracing

- **Endpoint**: `POST /trace`
- **Body**: `{ deviceId: string, trace_id: string, span_id: string, parent_span?: string, operation: string, duration_ms: number, started_at: number, ended_at: number, attributes?: object }`

## Configuration

Edit `ota_config.h` to configure the plugin:

```c
// Server Configuration
#define OTA_SERVER_BASE_URL "https://api.mybackend.com"
#define DEVICE_ID "esp32_device_001"
#define FIRMWARE_REF "v1.0.2"

// Intervals
#define OTA_CHECK_INTERVAL_MS 300000    // 5 minutes
#define OTA_HEARTBEAT_INTERVAL_MS 60000 // 1 minute

// Feature Flags
#define OTA_METRICS_ENABLED true
#define OTA_LOGGING_ENABLED true
#define OTA_TRACING_ENABLED true
```

## Usage

### Basic Integration

```c
#include "ota_plugin.h"

void app_main(void) {
    // Initialize WiFi first...

    // Initialize OTA plugin
    esp_err_t ret = ota_plugin_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OTA plugin");
        return;
    }

    // Start background tasks
    ret = ota_plugin_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start OTA plugin");
        return;
    }

    // Plugin is now running in background
}
```

### Logging

```c
// Send different log levels
ota_log(OTA_LOG_LEVEL_INFO, "Application started", NULL, "main");
ota_log(OTA_LOG_LEVEL_ERROR, "Sensor read failed", "stack_trace_here", "sensor_module");
```

### Custom Metrics

```c
// Send custom metrics (included in next heartbeat)
ota_plugin_send_metric("temperature", 25.6, "°C");
ota_plugin_send_metric("humidity", 65.2, "%");
```

### Tracing

```c
// Start a trace operation
ota_trace_context_t* trace = ota_trace_start("sensor_reading", NULL);

// Do some work...
perform_sensor_reading();

// End the trace
ota_trace_end(trace, "{\"sensor_type\":\"temperature\"}");
```

### Manual OTA Check

```c
// Trigger immediate OTA check
esp_err_t ret = ota_plugin_check_update();
```

## Built-in Metrics

The plugin automatically collects and sends these metrics:

- **battery_percentage**: Simulated battery level (20-100%)
- **wifi_signal_strength**: WiFi RSSI in dBm
- **free_heap_percentage**: Available heap memory percentage

## Dependencies

The plugin requires these ESP-IDF components:

- `esp_http_client`: HTTP client functionality
- `esp_https_ota`: OTA update capability
- `esp_wifi`: WiFi functionality
- `json`: JSON parsing (cJSON)
- `nvs_flash`: Non-volatile storage
- `esp_netif`: Network interface
- `esp_timer`: High-resolution timers
- `app_update`: Application update functionality

## Error Handling

The plugin includes comprehensive error handling:

- **Network failures**: Automatic retry with exponential backoff
- **OTA failures**: Status is saved and reported after reboot
- **Memory allocation**: Proper cleanup on failures
- **Invalid responses**: JSON parsing error handling

## Security

- **SSL/TLS**: All HTTP communications use SSL/TLS
- **Certificate validation**: Configurable certificate verification
- **Secure storage**: Sensitive data stored in NVS

## Performance

- **Non-blocking**: All operations run in background tasks
- **Memory efficient**: Minimal memory footprint
- **Configurable**: Task stack sizes and priorities are configurable

## Troubleshooting

### Common Issues

1. **WiFi Connection**: Ensure WiFi is connected before starting the plugin
2. **Server URL**: Verify the server URL is correct and accessible
3. **SSL Certificates**: Check SSL certificate validation settings
4. **Memory**: Monitor heap usage if experiencing stability issues

### Debug Logs

Enable debug logging for detailed information:

```c
esp_log_level_set("ota_plugin", ESP_LOG_DEBUG);
esp_log_level_set("ota_http_client", ESP_LOG_DEBUG);
esp_log_level_set("ota_status", ESP_LOG_DEBUG);
```

## License

This project is provided as-is for educational and development purposes.
