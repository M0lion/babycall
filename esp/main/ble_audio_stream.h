/**
 * @file ble_audio_stream.h
 * @brief BLE GATT Server for Baby Monitor Audio Streaming
 *
 * This module implements a BLE GATT server for streaming audio data from
 * an ESP32-C3 baby monitor device to a mobile application.
 *
 * IMPORTANT: Bluetooth must be enabled in menuconfig before using this module:
 *   - Component config -> Bluetooth -> [*] Bluetooth
 *   - Bluetooth -> Bluetooth Host -> Bluedroid - Dual-mode
 *   - Bluetooth -> Controller Options -> [*] Bluetooth controller
 */

#ifndef BLE_AUDIO_STREAM_H
#define BLE_AUDIO_STREAM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Service and Characteristic UUIDs (128-bit format)
 * Base UUID: 0000XXXX-0000-1000-8000-00805F9B34FB
 */
#define BLE_AUDIO_SERVICE_UUID          0x1900  // Baby Monitor Service
#define BLE_AUDIO_DATA_CHAR_UUID        0x1901  // Audio Data (NOTIFY)
#define BLE_AUDIO_CONFIG_CHAR_UUID      0x1902  // Audio Config (READ, WRITE)
#define BLE_AUDIO_CONTROL_CHAR_UUID     0x1903  // Control Commands (WRITE, NOTIFY)
#define BLE_AUDIO_DEVICE_INFO_CHAR_UUID 0x1904  // Device Info (READ)

/**
 * @brief Audio configuration structure
 * Binary format: [sample_rate(2B), bit_depth(1B), channels(1B), status(1B)]
 */
typedef struct {
    uint16_t sample_rate;  // Sample rate in Hz (8000, 16000)
    uint8_t bit_depth;     // Bit depth (16)
    uint8_t channels;      // Number of channels (1=mono, 2=stereo)
    uint8_t status;        // 0=stopped, 1=streaming
} __attribute__((packed)) ble_audio_config_t;

/**
 * @brief Control command IDs
 */
typedef enum {
    BLE_AUDIO_CMD_START_STREAM = 0x01,  // Start audio streaming
    BLE_AUDIO_CMD_STOP_STREAM  = 0x02,  // Stop audio streaming
    BLE_AUDIO_CMD_SET_GAIN     = 0x04   // Set audio gain (0-100)
} ble_audio_cmd_t;

/**
 * @brief Control command structure
 * Binary format: [cmd_id(1B), params_len(1B), params(variable)]
 */
typedef struct {
    uint8_t cmd_id;        // Command ID (ble_audio_cmd_t)
    uint8_t params_len;    // Length of params field
    uint8_t params[];      // Variable-length parameters
} __attribute__((packed)) ble_control_cmd_t;

/**
 * @brief Device information structure
 * Binary format: [fw_version(4B), battery_level(1B), uptime(4B)]
 */
typedef struct {
    uint32_t fw_version;      // Firmware version (e.g., 0x00010000 for v1.0.0)
    uint8_t battery_level;    // Battery level (0-100%)
    uint32_t uptime;          // Device uptime in seconds
} __attribute__((packed)) ble_device_info_t;

/**
 * @brief Connection state callback function type
 * @param connected true if client connected, false if disconnected
 */
typedef void (*ble_connection_cb_t)(bool connected);

/**
 * @brief Control command callback function type
 * @param cmd_id Command ID
 * @param params Pointer to parameters
 * @param params_len Length of parameters
 */
typedef void (*ble_control_cmd_cb_t)(uint8_t cmd_id, const uint8_t *params, uint8_t params_len);

/**
 * @brief Audio configuration callback function type
 * @param config Pointer to new audio configuration
 */
typedef void (*ble_audio_config_cb_t)(const ble_audio_config_t *config);

/**
 * @brief Initialize BLE stack and GATT server
 *
 * This function initializes the ESP-IDF Bluedroid stack, creates the
 * Baby Monitor GATT service with all characteristics, and prepares
 * the device for advertising.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_audio_init(void);

/**
 * @brief Start BLE advertising
 *
 * Starts advertising the device as "BabyMonitor-XXXX" where XXXX is
 * derived from the device MAC address.
 */
void ble_audio_start_advertising(void);

/**
 * @brief Stop BLE advertising
 */
void ble_audio_stop_advertising(void);

/**
 * @brief Send audio frame data via BLE notification
 *
 * Sends raw audio packet data to the connected client via BLE notifications.
 * The data should already include any necessary headers/formatting.
 *
 * @param data Pointer to audio data buffer
 * @param len Length of data in bytes (should be <= MTU - 3)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_audio_send_frame(const uint8_t *data, size_t len);

/**
 * @brief Check if a BLE client is connected
 *
 * @return true if client is connected, false otherwise
 */
bool ble_audio_is_connected(void);

/**
 * @brief Get the negotiated MTU size
 *
 * @return Current MTU size in bytes (default: 23, requested: 512)
 */
uint16_t ble_audio_get_mtu(void);

/**
 * @brief Register connection state change callback
 *
 * @param callback Callback function to be called on connection state changes
 */
void ble_audio_set_connection_callback(ble_connection_cb_t callback);

/**
 * @brief Register control command callback
 *
 * @param callback Callback function to be called when control commands are received
 */
void ble_audio_set_control_callback(ble_control_cmd_cb_t callback);

/**
 * @brief Register audio configuration change callback
 *
 * @param callback Callback function to be called when audio config is written
 */
void ble_audio_set_config_callback(ble_audio_config_cb_t callback);

/**
 * @brief Update the audio configuration
 *
 * Updates the internal audio configuration and notifies connected clients
 * if notifications are enabled.
 *
 * @param config Pointer to new audio configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_audio_update_config(const ble_audio_config_t *config);

/**
 * @brief Update device information
 *
 * Updates the internal device information that will be returned when
 * clients read the Device Info characteristic.
 *
 * @param info Pointer to new device information
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_audio_update_device_info(const ble_device_info_t *info);

/**
 * @brief Send a control command response
 *
 * Sends a response to a control command via notification on the control
 * characteristic. This can be used to acknowledge commands or report errors.
 *
 * @param cmd_id Original command ID
 * @param status Status code (0=success, non-zero=error)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_audio_send_control_response(uint8_t cmd_id, uint8_t status);

#ifdef __cplusplus
}
#endif

#endif // BLE_AUDIO_STREAM_H
