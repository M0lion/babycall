# BLEService Quick Start

## Installation

```bash
npm install react-native-ble-plx@^3.2.2
```

## Minimal Working Example

```typescript
import { useEffect, useState } from 'react';
import BLEService from '@/src/services/BLEService';
import { Device, ControlCommand } from '@/src/types';

function BabyMonitor() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    // Initialize on mount
    BLEService.init();
    BLEService.requestPermissions();

    // Cleanup on unmount
    return () => {
      BLEService.disconnect();
    };
  }, []);

  const startScan = async () => {
    setDevices([]);
    await BLEService.scanForDevices((device) => {
      setDevices(prev => [...prev, device]);
    }, 10000);
  };

  const connectToDevice = async (device: Device) => {
    BLEService.stopScan();
    await BLEService.connect(device);
    setConnected(true);

    // Setup disconnect handler
    BLEService.onDisconnect(() => {
      setConnected(false);
      console.log('Device disconnected!');
    });
  };

  const startMonitoring = async () => {
    // Start streaming on device
    await BLEService.sendControlCommand(ControlCommand.START_STREAM);

    // Start receiving audio data
    await BLEService.startAudioStream((audioData) => {
      // Parse protocol header
      const version = audioData[0];
      const sequence = audioData[1] | (audioData[2] << 8);
      const fragment = audioData[3];
      const length = audioData[4] | (audioData[5] << 8);
      const audio = audioData.slice(6, 6 + length);

      console.log(`Audio packet: seq=${sequence}, frag=${fragment}, len=${length}`);

      // TODO: Decode and play audio
    });
  };

  const stopMonitoring = async () => {
    BLEService.stopAudioStream();
    await BLEService.sendControlCommand(ControlCommand.STOP_STREAM);
  };

  return (
    <>
      <button onClick={startScan}>Scan for Devices</button>
      {devices.map(device => (
        <button key={device.id} onClick={() => connectToDevice(device)}>
          Connect to {device.name || device.id}
        </button>
      ))}
      {connected && (
        <>
          <button onClick={startMonitoring}>Start Monitoring</button>
          <button onClick={stopMonitoring}>Stop Monitoring</button>
        </>
      )}
    </>
  );
}
```

## Common Operations

### Initialize and Scan
```typescript
BLEService.init();
await BLEService.requestPermissions();
await BLEService.scanForDevices((device) => {
  console.log('Found:', device.name);
}, 10000);
```

### Connect
```typescript
await BLEService.connect(device);
BLEService.onDisconnect(() => console.log('Disconnected!'));
```

### Read Device Info
```typescript
const info = await BLEService.readDeviceInfo();
const version = `v${(info.firmwareVersion >> 24) & 0xFF}.${(info.firmwareVersion >> 16) & 0xFF}.${(info.firmwareVersion >> 8) & 0xFF}`;
console.log(`Firmware: ${version}, Battery: ${info.batteryLevel}%`);
```

### Start Audio Stream
```typescript
await BLEService.sendControlCommand(ControlCommand.START_STREAM);
await BLEService.startAudioStream((data) => {
  // Process audio data
});
```

### Stop Audio Stream
```typescript
BLEService.stopAudioStream();
await BLEService.sendControlCommand(ControlCommand.STOP_STREAM);
```

### Disconnect
```typescript
await BLEService.disconnect();
```

## Audio Data Format

Each audio packet contains:
```
[version][seq_low][seq_high][fragment][len_low][len_high][...audio_data...]
```

Parse like this:
```typescript
const version = audioData[0];
const sequence = audioData[1] | (audioData[2] << 8);
const fragment = audioData[3];
const length = audioData[4] | (audioData[5] << 8);
const audio = audioData.slice(6, 6 + length);
```

## Error Handling

Always use try/catch:
```typescript
try {
  await BLEService.connect(device);
} catch (error) {
  console.error('Failed to connect:', error);
  // Show error to user
}
```

## Platform Setup

### Android
Add to `android/app/src/main/AndroidManifest.xml`:
```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

### iOS
Add to `ios/YourApp/Info.plist`:
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Connect to baby monitor</string>
```

## Tips

1. Always call `init()` before using the service
2. Request permissions before scanning
3. Stop scanning before connecting
4. Use `onDisconnect()` to handle unexpected disconnections
5. Stop audio stream before disconnecting
6. Use `destroy()` when completely done with BLE

## Full Documentation

See `BLEService.README.md` for complete documentation and `BLEService.example.ts` for more examples.
