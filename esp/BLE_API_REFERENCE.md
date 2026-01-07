# BLE Audio Stream API Quick Reference

## Header File
```c
#include "ble_audio_stream.h"
```

## Initialization

### `ble_audio_init()`
```c
esp_err_t ble_audio_init(void);
```
- Initializes the Bluedroid BLE stack
- Creates GATT service with all characteristics
- Sets up default configurations
- **Returns:** `ESP_OK` on success

### `ble_audio_start_advertising()`
```c
void ble_audio_start_advertising(void);
```
- Starts BLE advertising
- Device name: "BabyMonitor-XXXX" (XXXX from MAC)
- Only starts if not already advertising or connected

### `ble_audio_stop_advertising()`
```c
void ble_audio_stop_advertising(void);
```
- Stops BLE advertising

## Data Transmission

### `ble_audio_send_frame()`
```c
esp_err_t ble_audio_send_frame(const uint8_t *data, size_t len);
```
- Sends audio data via BLE notification
- **Parameters:**
  - `data`: Pointer to audio buffer
  - `len`: Data length (must be ≤ MTU - 3)
- **Returns:** `ESP_OK` on success, `ESP_ERR_INVALID_STATE` if not connected

### `ble_audio_send_control_response()`
```c
esp_err_t ble_audio_send_control_response(uint8_t cmd_id, uint8_t status);
```
- Sends control command response/acknowledgment
- **Parameters:**
  - `cmd_id`: Original command ID
  - `status`: 0 = success, non-zero = error

## Status Queries

### `ble_audio_is_connected()`
```c
bool ble_audio_is_connected(void);
```
- Returns `true` if client is connected

### `ble_audio_get_mtu()`
```c
uint16_t ble_audio_get_mtu(void);
```
- Returns negotiated MTU size (default: 23, max: 512)
- Use `(mtu - 3)` to calculate max frame size

## Configuration Updates

### `ble_audio_update_config()`
```c
esp_err_t ble_audio_update_config(const ble_audio_config_t *config);
```
- Updates internal audio configuration
- **Structure:**
  ```c
  typedef struct {
      uint16_t sample_rate;  // 8000, 16000 Hz
      uint8_t bit_depth;     // 16
      uint8_t channels;      // 1=mono, 2=stereo
      uint8_t status;        // 0=stopped, 1=streaming
  } ble_audio_config_t;
  ```

### `ble_audio_update_device_info()`
```c
esp_err_t ble_audio_update_device_info(const ble_device_info_t *info);
```
- Updates device information
- **Structure:**
  ```c
  typedef struct {
      uint32_t fw_version;    // 0x00010000 = v1.0.0
      uint8_t battery_level;  // 0-100%
      uint32_t uptime;        // seconds
  } ble_device_info_t;
  ```

## Callbacks

### Connection State Callback
```c
typedef void (*ble_connection_cb_t)(bool connected);
void ble_audio_set_connection_callback(ble_connection_cb_t callback);
```
- Called when client connects/disconnects

### Control Command Callback
```c
typedef void (*ble_control_cmd_cb_t)(uint8_t cmd_id, const uint8_t *params, uint8_t params_len);
void ble_audio_set_control_callback(ble_control_cmd_cb_t callback);
```
- Called when control command is received
- Command IDs:
  - `0x01`: START_STREAM
  - `0x02`: STOP_STREAM
  - `0x04`: SET_GAIN (1 byte param: 0-100)

### Audio Config Callback
```c
typedef void (*ble_audio_config_cb_t)(const ble_audio_config_t *config);
void ble_audio_set_config_callback(ble_audio_config_cb_t callback);
```
- Called when client writes new audio configuration

## Complete Example

```c
#include "ble_audio_stream.h"
#include "esp_log.h"

static const char *TAG = "EXAMPLE";

// Connection callback
void on_connection(bool connected) {
    ESP_LOGI(TAG, "BLE %s", connected ? "connected" : "disconnected");
}

// Control command callback
void on_control(uint8_t cmd_id, const uint8_t *params, uint8_t params_len) {
    switch (cmd_id) {
        case BLE_AUDIO_CMD_START_STREAM:
            // Start streaming
            ble_audio_send_control_response(cmd_id, 0);
            break;
        case BLE_AUDIO_CMD_STOP_STREAM:
            // Stop streaming
            ble_audio_send_control_response(cmd_id, 0);
            break;
        case BLE_AUDIO_CMD_SET_GAIN:
            uint8_t gain = params[0];
            // Apply gain
            ble_audio_send_control_response(cmd_id, 0);
            break;
    }
}

void app_main(void) {
    // Initialize BLE
    esp_err_t ret = ble_audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Set callbacks
    ble_audio_set_connection_callback(on_connection);
    ble_audio_set_control_callback(on_control);

    // Start advertising
    ble_audio_start_advertising();

    // Main loop
    uint8_t audio_buffer[256];
    while (1) {
        if (ble_audio_is_connected()) {
            // Read/generate audio data
            size_t len = get_audio_samples(audio_buffer, sizeof(audio_buffer));

            // Send via BLE
            ret = ble_audio_send_frame(audio_buffer, len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Send failed: %s", esp_err_to_name(ret));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Service Specification

| UUID | Name | Properties | Description |
|------|------|------------|-------------|
| 0x1900 | Baby Monitor Service | - | Main service |
| 0x1901 | Audio Data | NOTIFY | Audio stream packets |
| 0x1902 | Audio Config | READ, WRITE | Audio configuration |
| 0x1903 | Control Command | WRITE, NOTIFY | Commands & responses |
| 0x1904 | Device Info | READ | Device information |

Full UUIDs use format: `0000XXXX-0000-1000-8000-00805F9B34FB`

## Connection Parameters

- **MTU Request:** 512 bytes
- **Connection Interval:** 7.5-15ms (low latency)
- **Slave Latency:** 0 (no latency)
- **Supervision Timeout:** 4 seconds

## Error Codes

| Code | Description |
|------|-------------|
| `ESP_OK` | Success |
| `ESP_ERR_INVALID_STATE` | Not connected or notifications disabled |
| `ESP_ERR_INVALID_SIZE` | Frame too large for MTU |
| `ESP_ERR_INVALID_ARG` | NULL pointer passed |

## Important Notes

1. **Enable Bluetooth** in menuconfig before building
2. **MTU Negotiation:** Actual MTU depends on client support
3. **Frame Size:** Use `ble_audio_get_mtu() - 3` for max data size
4. **Notifications:** Client must enable notifications to receive data
5. **Reconnection:** Advertising automatically restarts after disconnect
6. **Thread Safety:** BLE callbacks run in BLE task context

## Testing

Use the included Python test script:
```bash
pip install bleak
python test_ble_client.py
```

Or use mobile apps:
- **Android/iOS:** nRF Connect
- **Linux:** `gatttool` or `bluetoothctl`
- **macOS:** LightBlue
