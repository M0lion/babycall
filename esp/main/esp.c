/**
 * @file esp.c
 * @brief Baby Monitor Main Application
 *
 * This is the main application entry point for the ESP32-C3 baby monitor.
 * It initializes all required components and manages the audio streaming
 * pipeline over BLE.
 *
 * Architecture:
 * - NVS: Required for BLE stack operation
 * - BLE Audio Service: GATT server for audio streaming
 * - Audio Streamer: Manages I2S capture and BLE transmission
 *
 * @version 1.0.0
 */

#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_audio_stream.h"
#include "audio_streamer.h"

static const char *TAG = "BABY_MONITOR";

// Firmware version: v1.0.0
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
#define FW_VERSION ((FW_VERSION_MAJOR << 16) | (FW_VERSION_MINOR << 8) | FW_VERSION_PATCH)

/**
 * @brief Callback for BLE connection state changes
 *
 * Called when a BLE client connects or disconnects. The audio streamer
 * will automatically start/stop streaming based on connection state.
 *
 * @param connected true if client connected, false if disconnected
 */
static void on_ble_connection_changed(bool connected)
{
    if (connected) {
        ESP_LOGI(TAG, "BLE client connected - ready to stream audio");

        // Update audio config to indicate we're ready
        ble_audio_config_t config = {
            .sample_rate = 16000,
            .bit_depth = 16,
            .channels = 1,
            .status = 1  // Streaming active
        };
        ble_audio_update_config(&config);
    } else {
        ESP_LOGI(TAG, "BLE client disconnected - stopping stream");

        // Update audio config to indicate we've stopped
        ble_audio_config_t config = {
            .sample_rate = 16000,
            .bit_depth = 16,
            .channels = 1,
            .status = 0  // Not streaming
        };
        ble_audio_update_config(&config);
    }
}

/**
 * @brief Callback for BLE control commands
 *
 * Handles control commands received from the BLE client such as
 * start/stop streaming and gain adjustment.
 *
 * @param cmd_id Command ID
 * @param params Command parameters
 * @param params_len Length of parameters
 */
static void on_control_command(uint8_t cmd_id, const uint8_t *params, uint8_t params_len)
{
    ESP_LOGI(TAG, "Received control command: 0x%02X", cmd_id);

    switch (cmd_id) {
        case BLE_AUDIO_CMD_START_STREAM:
            ESP_LOGI(TAG, "Start streaming command received");
            // Audio streamer handles streaming automatically based on BLE connection
            ble_audio_send_control_response(cmd_id, 0); // Success
            break;

        case BLE_AUDIO_CMD_STOP_STREAM:
            ESP_LOGI(TAG, "Stop streaming command received");
            // Audio streamer handles streaming automatically based on BLE connection
            ble_audio_send_control_response(cmd_id, 0); // Success
            break;

        case BLE_AUDIO_CMD_SET_GAIN:
            if (params_len >= 1) {
                uint8_t gain = params[0];
                ESP_LOGI(TAG, "Set gain to %d (not implemented)", gain);
                // Gain control would be implemented here
                ble_audio_send_control_response(cmd_id, 0); // Success
            } else {
                ESP_LOGW(TAG, "Invalid gain parameter");
                ble_audio_send_control_response(cmd_id, 1); // Error
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown command: 0x%02X", cmd_id);
            ble_audio_send_control_response(cmd_id, 1); // Error
            break;
    }
}

/**
 * @brief Callback for audio configuration changes
 *
 * Called when the BLE client writes to the audio config characteristic.
 * This can be used to dynamically adjust audio parameters.
 *
 * @param config Pointer to new audio configuration
 */
static void on_audio_config_changed(const ble_audio_config_t *config)
{
    ESP_LOGI(TAG, "Audio config update: rate=%d Hz, bits=%d, channels=%d, status=%d",
             config->sample_rate, config->bit_depth, config->channels, config->status);

    // Note: Dynamic reconfiguration of I2S would require reinitialization
    // For now, we log the change but don't apply it
}

/**
 * @brief Initialize NVS (Non-Volatile Storage)
 *
 * NVS is required by the BLE stack to store pairing information and
 * other persistent data. This function initializes NVS and handles
 * error recovery.
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t init_nvs(void)
{
    ESP_LOGI(TAG, "Initializing NVS...");

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition is full or has incompatible version
        // Erase and retry initialization
        ESP_LOGW(TAG, "NVS partition needs to be erased (reason: %s)", esp_err_to_name(ret));
        ESP_LOGI(TAG, "Erasing NVS partition...");

        ESP_ERROR_CHECK(nvs_flash_erase());

        ESP_LOGI(TAG, "Retrying NVS initialization...");
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Main application entry point
 *
 * Initializes all components in the correct order and starts the
 * baby monitor audio streaming service.
 */
void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Baby Monitor v%d.%d.%d",
             FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    ESP_LOGI(TAG, "====================================");

    // Step 1: Initialize NVS (required for BLE)
    ret = init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed - cannot continue");
        return;
    }

    // Step 2: Initialize BLE audio service
    ESP_LOGI(TAG, "Initializing BLE audio service...");
    ret = ble_audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE audio: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "BLE audio service initialized successfully");

    // Step 3: Register BLE callbacks
    ESP_LOGI(TAG, "Registering BLE callbacks...");
    ble_audio_set_connection_callback(on_ble_connection_changed);
    ble_audio_set_control_callback(on_control_command);
    ble_audio_set_config_callback(on_audio_config_changed);

    // Step 4: Set initial device information
    ble_device_info_t device_info = {
        .fw_version = FW_VERSION,
        .battery_level = 100,  // 100% (no battery monitoring yet)
        .uptime = 0
    };
    ble_audio_update_device_info(&device_info);

    // Step 5: Set initial audio configuration
    ble_audio_config_t audio_config = {
        .sample_rate = 16000,
        .bit_depth = 16,
        .channels = 1,
        .status = 0  // Not streaming yet (will start when client connects)
    };
    ble_audio_update_config(&audio_config);

    // Step 6: Start BLE advertising
    ESP_LOGI(TAG, "Starting BLE advertising...");
    ble_audio_start_advertising();
    ESP_LOGI(TAG, "BLE advertising started - device is discoverable");

    // Step 7: Initialize audio streamer (initializes I2S internally)
    ESP_LOGI(TAG, "Initializing audio streamer...");
    ret = audio_streamer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio streamer: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Audio streamer initialized successfully");

    // Step 8: Start streaming task
    ESP_LOGI(TAG, "Starting audio streaming task...");
    ret = audio_streamer_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio streamer: %s", esp_err_to_name(ret));
        audio_streamer_deinit();
        return;
    }
    ESP_LOGI(TAG, "Audio streaming task started successfully");

    // Initialization complete
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Baby Monitor ready!");
    ESP_LOGI(TAG, "Waiting for BLE connections...");
    ESP_LOGI(TAG, "====================================");

    // Main monitoring loop - log statistics every 5 seconds
    uint32_t loop_count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        loop_count++;

        // Get streaming statistics
        audio_streamer_stats_t stats;
        if (audio_streamer_get_stats(&stats) == ESP_OK) {
            if (stats.is_streaming) {
                ESP_LOGI(TAG, "Stats [%us]: streaming=YES, frames=%llu, packets=%llu, "
                         "bytes=%llu, errors=%u, dropped=%u",
                         loop_count * 5,
                         stats.frames_sent,
                         stats.packets_sent,
                         stats.bytes_sent,
                         stats.errors,
                         stats.frames_dropped);
            } else {
                ESP_LOGD(TAG, "Stats [%us]: streaming=NO, waiting for connection...",
                         loop_count * 5);
            }
        }

        // Update device uptime every 5 seconds
        device_info.uptime = loop_count * 5;
        ble_audio_update_device_info(&device_info);
    }

    // Cleanup (never reached in normal operation)
    ESP_LOGI(TAG, "Shutting down...");
    audio_streamer_stop();
    audio_streamer_deinit();
    ble_audio_stop_advertising();
}
