# BLE Audio Streaming Setup Guide

This guide explains how to configure and use the BLE GATT server for baby monitor audio streaming on ESP32-C3.

## Prerequisites

- ESP-IDF v5.0 or later
- ESP32-C3 development board
- BLE-capable mobile device or computer for testing

## Enabling Bluetooth in SDK Configuration

Before building the project, Bluetooth must be enabled in the ESP-IDF configuration:

### Method 1: Using menuconfig (Recommended)

```bash
cd esp
idf.py menuconfig
```

Navigate through the menu and enable the following options:

1. **Component config → Bluetooth**
   - `[*]` Bluetooth

2. **Bluetooth → Bluetooth Host**
   - Select: `Bluedroid - Dual-mode`

3. **Bluetooth → Controller Options**
   - `[*]` Bluetooth controller
   - `[*]` BLE support

4. **Bluetooth → Bluedroid Options**
   - `[*]` Gatt Module Enable
   - MTU Size: Set to `512` (default is usually 23)

5. Save and exit (press 'S', then 'Q')

### Method 2: Using sdkconfig.defaults

Alternatively, create or modify `esp/sdkconfig.defaults` with these settings:

```
# Bluetooth Configuration
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y
CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=n
CONFIG_BTDM_CTRL_MODE_BTDM=n

# GATT Configuration
CONFIG_BT_GATTS_ENABLE=y
CONFIG_BT_GATT_MAX_SR_ATTRIBUTES=40
CONFIG_BT_ACL_CONNECTIONS=4

# MTU Configuration
CONFIG_BT_GATT_MAX_MTU_SIZE=512
```

## Building the Project

After enabling Bluetooth:

```bash
cd esp
idf.py build
idf.py flash monitor
```

## BLE Service Specification

### Service UUID
- **Service**: `00001900-0000-1000-8000-00805F9B34FB` (Baby Monitor Service)

### Characteristics

| UUID | Name | Properties | Description |
|------|------|------------|-------------|
| `0x1901` | Audio Data | NOTIFY | Audio stream data packets |
| `0x1902` | Audio Config | READ, WRITE | Audio configuration |
| `0x1903` | Control Command | WRITE, NOTIFY | Control commands and responses |
| `0x1904` | Device Info | READ | Device information |

### Data Formats

#### Audio Config (0x1902)
Binary format (5 bytes):
```
[sample_rate(2B), bit_depth(1B), channels(1B), status(1B)]
```
- `sample_rate`: 8000 or 16000 Hz (uint16_t, little-endian)
- `bit_depth`: 16 (uint8_t)
- `channels`: 1=mono (uint8_t)
- `status`: 0=stopped, 1=streaming (uint8_t)

#### Control Command (0x1903)
Binary format (variable length):
```
[cmd_id(1B), params_len(1B), params(variable)]
```

Command IDs:
- `0x01`: START_STREAM (no params)
- `0x02`: STOP_STREAM (no params)
- `0x04`: SET_GAIN (params: gain value 0-100)

#### Device Info (0x1904)
Binary format (9 bytes):
```
[fw_version(4B), battery_level(1B), uptime(4B)]
```
- `fw_version`: Firmware version (uint32_t, e.g., 0x00010000 = v1.0.0)
- `battery_level`: Battery percentage 0-100 (uint8_t)
- `uptime`: Device uptime in seconds (uint32_t)

## API Usage Examples

### Example 1: Basic BLE Initialization

```c
#include "ble_audio_stream.h"

// Initialize BLE service
esp_err_t ret = ble_audio_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize BLE: %s", esp_err_to_name(ret));
    return;
}

// Start advertising
ble_audio_start_advertising();
```

### Example 2: Handling Connection Events

```c
// Connection state callback
static void on_connection_changed(bool connected) {
    if (connected) {
        ESP_LOGI(TAG, "Client connected");
        // Start audio processing
    } else {
        ESP_LOGI(TAG, "Client disconnected");
        // Stop audio processing
    }
}

// Register callback
ble_audio_set_connection_callback(on_connection_changed);
```

### Example 3: Handling Control Commands

```c
// Control command callback
static void on_control_command(uint8_t cmd_id, const uint8_t *params, uint8_t params_len) {
    switch (cmd_id) {
        case BLE_AUDIO_CMD_START_STREAM:
            // Start streaming logic
            ble_audio_send_control_response(cmd_id, 0); // Success
            break;

        case BLE_AUDIO_CMD_STOP_STREAM:
            // Stop streaming logic
            ble_audio_send_control_response(cmd_id, 0);
            break;

        case BLE_AUDIO_CMD_SET_GAIN:
            uint8_t gain = params[0];
            // Apply gain
            ble_audio_send_control_response(cmd_id, 0);
            break;
    }
}

// Register callback
ble_audio_set_control_callback(on_control_command);
```

### Example 4: Streaming Audio Data

```c
// Check if connected
if (ble_audio_is_connected()) {
    // Get maximum frame size based on MTU
    uint16_t mtu = ble_audio_get_mtu();
    size_t max_frame_size = mtu - 3; // 3 bytes overhead for BLE

    // Send audio frame
    uint8_t audio_data[max_frame_size];
    // ... fill audio_data with PCM samples ...

    esp_err_t ret = ble_audio_send_frame(audio_data, max_frame_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send frame: %s", esp_err_to_name(ret));
    }
}
```

### Example 5: Updating Device Information

```c
// Update device info periodically
ble_device_info_t info = {
    .fw_version = 0x00010000,    // v1.0.0
    .battery_level = 85,          // 85%
    .uptime = esp_timer_get_time() / 1000000  // seconds
};

ble_audio_update_device_info(&info);
```

## Connection Parameters

The BLE service uses the following optimized connection parameters for low-latency audio streaming:

- **MTU Request**: 512 bytes (negotiated with client)
- **Connection Interval**: 7.5-15ms (0x06-0x0C)
- **Slave Latency**: 0 (no latency for real-time audio)
- **Supervision Timeout**: 4 seconds

## Testing with BLE Tools

### Using nRF Connect (Mobile App)

1. Install nRF Connect on your mobile device
2. Scan for devices
3. Connect to "BabyMonitor-XXXX"
4. Explore the Baby Monitor Service (UUID: 0x1900)
5. Enable notifications on Audio Data characteristic
6. Write control commands to start streaming

### Using gatttool (Linux)

```bash
# Connect to device
gatttool -b AA:BB:CC:DD:EE:FF -I

# In gatttool prompt:
connect
char-desc
char-read-uuid 0x1904  # Read device info
char-write-req <handle> 01020000  # Write control command
```

## Troubleshooting

### Bluetooth Not Available

**Error**: `Failed to initialize BT controller`

**Solution**: Ensure Bluetooth is enabled in sdkconfig:
```bash
idf.py menuconfig
# Enable Bluetooth as described above
```

### MTU Negotiation Issues

**Error**: MTU stays at 23 bytes

**Solution**:
- Ensure `CONFIG_BT_GATT_MAX_MTU_SIZE=512` in sdkconfig
- Check that the client also supports larger MTU
- Some platforms limit MTU to 185 or 247 bytes

### Connection Drops

**Issue**: Frequent disconnections during streaming

**Solution**:
- Reduce data rate or frame size
- Check interference on 2.4 GHz band
- Ensure adequate power supply to ESP32-C3
- Monitor ESP_LOG messages for BLE stack errors

### Memory Issues

**Error**: `E (12345) BT_APPL: gatt_c_cl_init: No resources`

**Solution**: Increase heap size in menuconfig:
```
Component config → ESP32-specific → System event task stack size
```

## Performance Considerations

### Audio Quality vs. Latency

- **Higher MTU**: Better throughput, lower overhead
- **Shorter connection interval**: Lower latency, higher power consumption
- **Frame size**: Balance between latency and efficiency

### Recommended Settings for Real-time Audio

- Sample rate: 16 kHz
- Bit depth: 16-bit
- Channels: Mono
- Frame size: 128-256 samples (256-512 bytes)
- Effective latency: ~30-50ms

## Integration with I2S Audio

See the commented example in `esp.c` for a complete integration showing:
- Initializing both I2S and BLE
- Reading audio from microphone
- Streaming via BLE when connected
- Handling connection state changes

## Additional Resources

- [ESP-IDF Bluetooth Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)
- [BLE GATT Specification](https://www.bluetooth.com/specifications/gatt/)
- ESP32-C3 Datasheet
