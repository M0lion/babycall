# BLEService Implementation

Complete BLE service layer for connecting to the ESP32 baby monitor device.

## Files Created

1. `/home/bjorn/projects/babycall/app/src/types/ble.types.ts` - TypeScript type definitions
2. `/home/bjorn/projects/babycall/app/src/services/BLEService.ts` - Main BLE service implementation
3. `/home/bjorn/projects/babycall/app/src/services/BLEService.example.ts` - Usage examples
4. `/home/bjorn/projects/babycall/app/src/types/index.ts` - Type exports
5. `/home/bjorn/projects/babycall/app/src/services/index.ts` - Service exports

## Installation

### 1. Install react-native-ble-plx

```bash
cd /home/bjorn/projects/babycall/app
npm install react-native-ble-plx@^3.2.2
```

### 2. Android Configuration

Add the following permissions to `android/app/src/main/AndroidManifest.xml`:

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <!-- Bluetooth permissions for Android 12+ -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

    <!-- Bluetooth permissions for Android 11 and below -->
    <uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />

    <!-- Location permission required for BLE scanning -->
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

    <application>
        <!-- Your application config -->
    </application>
</manifest>
```

### 3. iOS Configuration

Add the following keys to `ios/YourApp/Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app needs Bluetooth to connect to the baby monitor</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>This app needs Bluetooth to connect to the baby monitor</string>
```

## Usage

### Basic Example

```typescript
import BLEService from '@/src/services/BLEService';
import { Device, ControlCommand } from '@/src/types';

// Initialize
BLEService.init();

// Request permissions (Android)
const granted = await BLEService.requestPermissions();

// Scan for devices
await BLEService.scanForDevices((device: Device) => {
  console.log('Found:', device.name);
}, 10000);

// Connect
await BLEService.connect(device);

// Read device info
const info = await BLEService.readDeviceInfo();

// Start streaming
await BLEService.sendControlCommand(ControlCommand.START_STREAM);
await BLEService.startAudioStream((audioData) => {
  // Process audio data
  console.log('Audio:', audioData.length, 'bytes');
});

// Stop streaming
BLEService.stopAudioStream();
await BLEService.sendControlCommand(ControlCommand.STOP_STREAM);

// Disconnect
await BLEService.disconnect();
```

## API Reference

### Initialization

#### `init(): void`
Initialize the BLE manager. Must be called before any other methods.

#### `requestPermissions(): Promise<boolean>`
Request Bluetooth permissions (Android only). Returns true if granted.

### Scanning

#### `scanForDevices(onDeviceFound: (device: Device) => void, timeoutMs?: number): Promise<void>`
Scan for devices with service UUID 0x1900.
- `onDeviceFound` - Callback invoked when a device is found
- `timeoutMs` - Scan timeout in milliseconds (default: 10000)

#### `stopScan(): void`
Stop scanning for devices.

### Connection

#### `connect(device: Device): Promise<void>`
Connect to a device and discover services. Automatically negotiates MTU (512 bytes).

#### `disconnect(): Promise<void>`
Disconnect from the connected device.

#### `isConnected(): boolean`
Check if a device is currently connected.

#### `getConnectedDevice(): Device | null`
Get the currently connected device.

#### `onDisconnect(callback: () => void): void`
Set callback for disconnect events.

### Audio Streaming

#### `startAudioStream(onAudioData: (data: Uint8Array) => void): Promise<void>`
Start monitoring audio data characteristic for notifications.

Audio data format: `[version|sequence|fragment|length|audio_data]`
- `version` (1 byte) - Protocol version
- `sequence` (2 bytes) - Packet sequence number (little-endian)
- `fragment` (1 byte) - Fragment index
- `length` (2 bytes) - Audio data length (little-endian)
- `audio_data` (variable) - Raw audio data

#### `stopAudioStream(): void`
Stop monitoring audio data characteristic.

### Control Commands

#### `sendControlCommand(command: number, params?: Uint8Array): Promise<void>`
Send a control command to the device.

Available commands:
- `ControlCommand.START_STREAM` (0x01) - Start audio streaming
- `ControlCommand.STOP_STREAM` (0x02) - Stop audio streaming
- `ControlCommand.SET_GAIN` (0x04) - Set gain (requires 1 byte parameter)

### Device Information

#### `readAudioConfig(): Promise<AudioConfig>`
Read audio configuration from the device.

Returns:
```typescript
{
  sampleRate: number;    // Sample rate in Hz (uint16_t)
  bitDepth: number;      // Bit depth (uint8_t)
  channels: number;      // Number of channels - 1=mono (uint8_t)
  status: number;        // Status - 0=stopped, 1=streaming (uint8_t)
}
```

#### `readDeviceInfo(): Promise<DeviceInfo>`
Read device information from the device.

Returns:
```typescript
{
  firmwareVersion: number;  // Firmware version (uint32_t) - e.g., 0x00010203 = v1.2.3
  batteryLevel: number;     // Battery level 0-100 (uint8_t)
  uptime: number;           // Uptime in seconds (uint32_t)
}
```

### Cleanup

#### `destroy(): void`
Destroy the BLE manager and clean up all resources.

## Type Definitions

### BLE UUIDs

```typescript
const BLE_UUIDS = {
  SERVICE: '00001900-0000-1000-8000-00805F9B34FB',
  AUDIO_DATA: '00001901-0000-1000-8000-00805F9B34FB',
  AUDIO_CONFIG: '00001902-0000-1000-8000-00805F9B34FB',
  CONTROL: '00001903-0000-1000-8000-00805F9B34FB',
  DEVICE_INFO: '00001904-0000-1000-8000-00805F9B34FB',
};
```

### Binary Protocol

All multi-byte integers use **little-endian** byte order to match ESP32.

#### Audio Config (5 bytes)
```
[sampleRate_low][sampleRate_high][bitDepth][channels][status]
```

#### Device Info (9 bytes)
```
[fw_byte0][fw_byte1][fw_byte2][fw_byte3][batteryLevel][uptime_byte0][uptime_byte1][uptime_byte2][uptime_byte3]
```

#### Control Command (1+ bytes)
```
[command][param1][param2]...
```

## Implementation Details

### Features
- Singleton pattern for global access
- Permission handling for Android (runtime permissions)
- MTU negotiation (512 bytes)
- Proper error handling with try/catch
- Debug logging for all operations
- Disconnect event monitoring
- Base64 ↔ Uint8Array conversion
- Little-endian binary parsing

### Error Handling
All async methods throw errors on failure. Use try/catch:

```typescript
try {
  await BLEService.connect(device);
} catch (error) {
  console.error('Connection failed:', error);
}
```

### Logging
All operations are logged with `[BLEService]` prefix for easy debugging.

### Thread Safety
The service uses a singleton pattern but is not thread-safe. All operations should be called from the main/UI thread.

### State Management
- Only one device can be connected at a time
- Only one audio stream can be active at a time
- Scanning stops automatically after timeout or when `stopScan()` is called
- Disconnecting automatically stops audio streaming

## Troubleshooting

### "BLE Manager not initialized"
Call `BLEService.init()` before any other methods.

### "Bluetooth is not powered on"
Ensure Bluetooth is enabled on the device.

### "Permissions not granted"
Request permissions using `BLEService.requestPermissions()` and handle the case where user denies.

### "No device connected"
Ensure you successfully called `connect()` before attempting to read/write characteristics.

### "MTU negotiation failed"
This is a warning, not an error. The service continues with the default MTU. Some Android devices don't support MTU negotiation.

### Type errors during compilation
Ensure `react-native-ble-plx@^3.2.2` is installed:
```bash
npm install react-native-ble-plx@^3.2.2
```

## Next Steps

1. Install `react-native-ble-plx` dependency
2. Configure platform-specific permissions
3. Integrate BLEService into your React components
4. Implement audio decoding/playback logic
5. Add UI for device scanning and connection
6. Handle edge cases (permissions denied, Bluetooth off, etc.)

## See Also

- `/home/bjorn/projects/babycall/app/src/services/BLEService.example.ts` - Complete usage examples
- [react-native-ble-plx Documentation](https://github.com/dotintent/react-native-ble-plx)
- [ESP32 BLE Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)
