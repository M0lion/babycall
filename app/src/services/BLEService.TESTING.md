# BLEService Testing Checklist

## Pre-Testing Setup

- [ ] Install react-native-ble-plx: `npm install react-native-ble-plx@^3.2.2`
- [ ] Add Android permissions to AndroidManifest.xml
- [ ] Add iOS permissions to Info.plist
- [ ] Rebuild the app after adding native dependencies
- [ ] Ensure Bluetooth is enabled on test device
- [ ] Have ESP32 baby monitor powered on and nearby

## Unit Testing Checklist

### Initialization
- [ ] `init()` creates BleManager instance
- [ ] Calling `init()` twice doesn't create duplicate managers
- [ ] `requestPermissions()` returns true on iOS
- [ ] `requestPermissions()` shows permission dialog on Android
- [ ] `requestPermissions()` handles user denial gracefully

### Scanning
- [ ] `scanForDevices()` finds ESP32 device with service UUID 0x1900
- [ ] `onDeviceFound` callback is invoked for each discovered device
- [ ] Scan stops automatically after timeout
- [ ] `stopScan()` stops scanning immediately
- [ ] Multiple calls to `stopScan()` don't cause errors
- [ ] Device list doesn't contain duplicates

### Connection
- [ ] `connect()` successfully connects to device
- [ ] `connect()` discovers all services and characteristics
- [ ] MTU negotiation succeeds (or fails gracefully)
- [ ] `isConnected()` returns true after successful connection
- [ ] `getConnectedDevice()` returns connected device
- [ ] `connect()` with invalid device throws error
- [ ] Connecting while already connected throws error

### Disconnect Handling
- [ ] `disconnect()` cleanly disconnects from device
- [ ] `isConnected()` returns false after disconnect
- [ ] `getConnectedDevice()` returns null after disconnect
- [ ] `onDisconnect()` callback is invoked on unexpected disconnect
- [ ] Audio stream stops automatically on disconnect
- [ ] Disconnecting while not connected doesn't throw error

### Audio Streaming
- [ ] `startAudioStream()` subscribes to audio data characteristic
- [ ] `onAudioData` callback receives Uint8Array data
- [ ] Audio data has correct protocol format (version|seq|frag|len|data)
- [ ] Audio data length matches the length field in header
- [ ] Multiple audio packets are received in sequence
- [ ] `stopAudioStream()` unsubscribes from characteristic
- [ ] Starting stream while already streaming replaces subscription

### Control Commands
- [ ] `sendControlCommand(START_STREAM)` succeeds
- [ ] `sendControlCommand(STOP_STREAM)` succeeds
- [ ] `sendControlCommand(SET_GAIN, [value])` succeeds with parameter
- [ ] Invalid command doesn't crash the app
- [ ] Control command returns after write completes

### Reading Configuration
- [ ] `readAudioConfig()` returns valid AudioConfig
- [ ] AudioConfig has expected sample rate (e.g., 16000)
- [ ] AudioConfig has expected bit depth (e.g., 16)
- [ ] AudioConfig has expected channels (1 for mono)
- [ ] AudioConfig status reflects streaming state
- [ ] Reading config while not connected throws error

### Reading Device Info
- [ ] `readDeviceInfo()` returns valid DeviceInfo
- [ ] DeviceInfo has non-zero firmware version
- [ ] DeviceInfo battery level is between 0-100
- [ ] DeviceInfo uptime increases over time
- [ ] Reading info while not connected throws error

### Error Handling
- [ ] Operations before `init()` throw descriptive errors
- [ ] Operations before connection throw "No device connected"
- [ ] Network errors are caught and thrown with context
- [ ] BLE errors include original error message
- [ ] All async methods reject promises on failure

### Resource Cleanup
- [ ] `destroy()` removes all subscriptions
- [ ] `destroy()` destroys BleManager
- [ ] `destroy()` clears connected device reference
- [ ] App doesn't leak memory after multiple connect/disconnect cycles

## Integration Testing

### Complete Workflow
- [ ] Init → Scan → Connect → Read Info → Stream → Disconnect workflow succeeds
- [ ] Can reconnect to same device after disconnect
- [ ] Can connect to different device after disconnect
- [ ] App handles ESP32 going out of range gracefully
- [ ] App handles ESP32 reboot gracefully
- [ ] App handles phone Bluetooth toggle gracefully

### Concurrent Operations
- [ ] Can't scan and connect simultaneously
- [ ] Can't connect to multiple devices
- [ ] Can't have multiple audio streams
- [ ] Disconnect cancels pending operations

### Edge Cases
- [ ] No devices found during scan (timeout)
- [ ] Connection fails (device out of range)
- [ ] Connection drops during audio stream
- [ ] ESP32 battery dies during streaming
- [ ] Phone goes to background during streaming
- [ ] Phone receives call during streaming

## Performance Testing

- [ ] Scan completes within timeout period
- [ ] Connection completes within 5 seconds
- [ ] Audio packets arrive with minimal latency (<100ms)
- [ ] No memory leaks after 1 hour of streaming
- [ ] CPU usage is reasonable during streaming
- [ ] Battery drain is acceptable

## Platform-Specific Testing

### Android
- [ ] Works on Android 12+ (BLUETOOTH_SCAN/CONNECT permissions)
- [ ] Works on Android 11 and below (BLUETOOTH/BLUETOOTH_ADMIN)
- [ ] Permission dialog shows and handles all outcomes
- [ ] Location permission is requested
- [ ] Works with location services disabled (Android 12+)

### iOS
- [ ] Bluetooth permission dialog shows
- [ ] Permission denial shows alert
- [ ] Background mode (if enabled) works correctly
- [ ] App Store privacy manifest is correct

## ESP32 Compatibility Testing

- [ ] Service UUID 0x1900 is discovered
- [ ] All characteristics (0x1901-0x1904) are accessible
- [ ] Audio data characteristic supports notifications
- [ ] Audio config characteristic supports read
- [ ] Control characteristic supports write with response
- [ ] Device info characteristic supports read
- [ ] MTU negotiation to 512 bytes succeeds

## Binary Protocol Testing

### Audio Config Parsing
- [ ] Sample rate little-endian parsing is correct
- [ ] Bit depth byte is correct
- [ ] Channels byte is correct
- [ ] Status byte is correct
- [ ] Total packet is 5 bytes

### Device Info Parsing
- [ ] Firmware version little-endian parsing is correct
- [ ] Battery level byte is correct
- [ ] Uptime little-endian parsing is correct
- [ ] Total packet is 9 bytes

### Audio Data Format
- [ ] Version byte is parsed correctly
- [ ] Sequence number little-endian parsing is correct
- [ ] Fragment byte is parsed correctly
- [ ] Length little-endian parsing is correct
- [ ] Audio data extraction is correct

## Regression Testing

After any changes to BLEService.ts:
- [ ] Re-run all unit tests
- [ ] Re-run integration workflow test
- [ ] Verify no new TypeScript errors
- [ ] Verify no performance degradation
- [ ] Test on both Android and iOS

## Test Tools

### Manual Testing
```typescript
// In a test component
import BLEService from '@/src/services/BLEService';

// Log all operations
BLEService.init();
console.log('Initialized');

// Test each method systematically
```

### Automated Testing
```typescript
// Using Jest or React Native Testing Library
describe('BLEService', () => {
  beforeEach(() => {
    BLEService.init();
  });

  afterEach(() => {
    BLEService.destroy();
  });

  it('should initialize without errors', () => {
    expect(BLEService.isConnected()).toBe(false);
  });

  // Add more tests...
});
```

## Known Issues / Limitations

Document any issues found during testing:

- [ ] MTU negotiation may fail on some Android devices (this is normal)
- [ ] iOS background mode requires additional configuration
- [ ] Some devices may require longer scan timeout
- [ ] (Add more as discovered)

## Testing Log

| Date | Tester | Platform | Version | Pass/Fail | Notes |
|------|--------|----------|---------|-----------|-------|
|      |        |          |         |           |       |

## Sign-off

- [ ] All critical tests pass
- [ ] All edge cases handled
- [ ] Error messages are clear and actionable
- [ ] Performance is acceptable
- [ ] Documentation is accurate
- [ ] Ready for production use
