#ifndef BLE_SIMPLE_H
#define BLE_SIMPLE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_SIMPLE_MAX_SERVICES 4
#define BLE_SIMPLE_MAX_CHARACTERISTICS 16
#define BLE_SIMPLE_MAX_CHAR_VALUE_LEN 512

// Forward declaration
typedef struct ble_char_handle *ble_char_handle_t;

// Callback types
typedef void (*ble_read_cb_t)(ble_char_handle_t handle, uint8_t *out_data,
                              uint16_t *out_len);
typedef void (*ble_write_cb_t)(ble_char_handle_t handle, const uint8_t *data,
                               uint16_t len);
typedef void (*ble_connect_cb_t)(bool connected);

// Configuration for initialization
typedef struct {
  const char *device_name;
  ble_connect_cb_t on_connect; // Optional: called on connect/disconnect
} ble_simple_config_t;

// Characteristic configuration
typedef struct {
  uint8_t uuid[16];        // 128-bit UUID (or use helper macros below)
  ble_read_cb_t on_read;   // NULL if not readable
  ble_write_cb_t on_write; // NULL if not writable
  bool notifications;      // Enable notifications
  bool indicate;    // Enable indications (like notifications but with ACK)
  uint16_t max_len; // Max value length (default: 512 if 0)
} ble_char_config_t;

/**
 * Initialize BLE with the given device name
 */
esp_err_t ble_simple_init(const ble_simple_config_t *config);

/**
 * Create a service with the given 128-bit UUID
 * Returns service index for use with ble_simple_add_characteristic
 */
esp_err_t ble_simple_add_service(const uint8_t uuid[16], int *out_service_idx);

/**
 * Add a characteristic to a service
 * Returns handle for use with notifications
 */
esp_err_t ble_simple_add_characteristic(int service_idx,
                                        const ble_char_config_t *config,
                                        ble_char_handle_t *out_handle);

/**
 * Start advertising (call after all services/characteristics are registered)
 */
esp_err_t ble_simple_start_advertising(void);

/**
 * Stop advertising
 */
esp_err_t ble_simple_stop_advertising(void);

/**
 * Send a notification/indication to connected client
 */
esp_err_t ble_simple_notify(ble_char_handle_t handle, const uint8_t *data,
                            uint16_t len);

/**
 * Check if a client is connected
 */
bool ble_simple_is_connected(void);

/**
 * Deinitialize BLE
 */
esp_err_t ble_simple_deinit(void);

// ============ Helper macros for UUIDs ============

// Create a 128-bit UUID from a 16-bit short UUID (Bluetooth SIG base)
#define BLE_UUID16_TO_128(uuid16)                                              \
  {0xFB,                                                                       \
   0x34,                                                                       \
   0x9B,                                                                       \
   0x5F,                                                                       \
   0x80,                                                                       \
   0x00,                                                                       \
   0x00,                                                                       \
   0x80,                                                                       \
   0x00,                                                                       \
   0x10,                                                                       \
   0x00,                                                                       \
   0x00,                                                                       \
   (uuid16) & 0xFF,                                                            \
   ((uuid16) >> 8) & 0xFF,                                                     \
   0x00,                                                                       \
   0x00}

// Create a custom 128-bit UUID (provide all 16 bytes)
#define BLE_UUID128(...) {__VA_ARGS__}

#ifdef __cplusplus
}
#endif

#endif // BLE_SIMPLE_H
