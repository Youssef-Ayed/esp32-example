#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// Server Configuration
#define OTA_SERVER_BASE_URL "http://192.168.10.149:5000/api" // Backend server URL
#define OTA_SERVER_TIMEOUT_MS 150000                         // 150 seconds timeout for HTTP requests
#define OTA_MAX_HTTP_OUTPUT_BUFFER 2048                      // Maximum buffer size for HTTP responses

// Device Configuration
#define DEVICE_ID "Test_Device_001"   // This can be generated or flashed
#define FIRMWARE_REF "esp32-devboard" // Current firmware reference, can be updated dynamically
#define OTA_FIRMWARE_VERSION "5.0.0"  // Initial firmware version, updated dynamically

// OTA Configuration
#define OTA_CHECK_INTERVAL_MS 300000    // Check for updates every 5 minutes
#define OTA_HEARTBEAT_INTERVAL_MS 60000 // Heartbeat every 60 seconds
#define OTA_MAX_RETRY_COUNT 3           // Maximum retries for OTA operations
#define OTA_RETRY_DELAY_MS 5000         // Delay between retries

// Feature Flags
#define OTA_METRICS_ENABLED true   // Enable metrics collection
#define OTA_LOGGING_ENABLED true   // Enable logging to remote server
#define OTA_TRACING_ENABLED true   // Enable tracing for operations
#define OTA_SSL_VERIFICATION false // Enable SSL verification for secure connections

// Task Configuration
#define OTA_TASK_STACK_SIZE 8192           // Stack size for OTA tasks
#define OTA_TASK_PRIORITY 5                // Priority for OTA tasks
#define OTA_HEARTBEAT_TASK_STACK_SIZE 4096 // Stack size for heartbeat task
#define OTA_HEARTBEAT_TASK_PRIORITY 3      // Priority for heartbeat task

// Buffer Sizes
#define OTA_JSON_BUFFER_SIZE 1024   // Buffer size for JSON data
#define OTA_URL_BUFFER_SIZE 512     // Buffer size for URLs
#define OTA_MESSAGE_BUFFER_SIZE 512 // Buffer size for messages
#define OTA_TRACE_ID_SIZE 32        // Buffer size for trace IDs
#define OTA_SPAN_ID_SIZE 16         // Buffer size for span IDs

    // Log Levels
    typedef enum
    {
        OTA_LOG_LEVEL_INFO,
        OTA_LOG_LEVEL_WARN,
        OTA_LOG_LEVEL_ERROR,
        OTA_LOG_LEVEL_FATAL
    } ota_log_level_t;

    // OTA Status
    typedef enum
    {
        OTA_STATUS_IDLE,
        OTA_STATUS_CHECKING,
        OTA_STATUS_DOWNLOADING,
        OTA_STATUS_INSTALLING,
        OTA_STATUS_SUCCESS,
        OTA_STATUS_FAILED
    } ota_status_t;

#ifdef __cplusplus
}
#endif

#endif // OTA_CONFIG_H