# Baby Monitor App Setup Guide

Complete setup and configuration guide for the Baby Monitor mobile application.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Configuration](#configuration)
4. [Platform-Specific Setup](#platform-specific-setup)
5. [Building the App](#building-the-app)
6. [Testing](#testing)
7. [Troubleshooting](#troubleshooting)
8. [ESP32 Device Setup](#esp32-device-setup)

---

## Prerequisites

### Required Software

- **Node.js**: v18.x or later (LTS recommended)
- **npm**: v9.x or later (comes with Node.js)
- **Expo CLI**: Automatically installed via npx

### Platform-Specific Requirements

#### For iOS Development
- **macOS** with Xcode 14.0 or later
- **iOS 13.4** or later device/simulator
- **Apple Developer Account** (for physical device testing)
- **CocoaPods**: v1.10 or later

#### For Android Development
- **Android Studio** with SDK Platform 33 or later
- **Android 12 (API 31)** or later device/emulator
- **Java Development Kit (JDK)**: 11 or later

### Hardware Requirements

- **Physical devices required** for BLE and audio testing
  - BLE does not work reliably in iOS Simulator
  - Android emulator BLE support is limited
- **ESP32-C3** development board with baby monitor firmware

---

## Installation

### 1. Clone and Navigate to App Directory

```bash
cd /home/bjorn/projects/babycall/app
```

### 2. Install Dependencies

```bash
npm install
```

This will install all required packages including:
- `expo` (v54.0.31)
- `react-native` (v0.81.5)
- `react-native-ble-plx` (v3.2.2) - Bluetooth Low Energy
- `react-native-audio-api` (v0.3.7) - Audio playback
- All Expo and React Native ecosystem packages

### 3. Verify Installation

```bash
npx expo --version
```

Expected output: `54.0.31` or similar

---

## Configuration

### App Configuration (`app.json`)

The app is pre-configured with all necessary plugins and permissions:

#### BLE Plugin Configuration
```json
{
  "react-native-ble-plx": {
    "isBackgroundEnabled": true,
    "modes": ["peripheral", "central"],
    "bluetoothAlwaysUsageDescription": "Connect to baby monitor device for audio streaming"
  }
}
```

#### Audio Plugin Configuration
```json
{
  "react-native-audio-api": {
    "iosBackgroundMode": true,
    "androidForegroundService": true,
    "androidFSTypes": ["mediaPlayback"],
    "androidPermissions": [
      "android.permission.MODIFY_AUDIO_SETTINGS",
      "android.permission.FOREGROUND_SERVICE",
      "android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK"
    ]
  }
}
```

### Permissions Summary

#### iOS Permissions (Info.plist)
✅ Already configured via `app.json`:
- `NSBluetoothAlwaysUsageDescription` - BLE access
- `NSBluetoothPeripheralUsageDescription` - BLE peripheral mode
- `NSMicrophoneUsageDescription` - Audio monitoring
- `UIBackgroundModes`: `audio`, `bluetooth-central` - Background operation

#### Android Permissions
✅ Automatically handled by plugins:
- `BLUETOOTH_SCAN` - Scan for BLE devices (API 31+)
- `BLUETOOTH_CONNECT` - Connect to BLE devices (API 31+)
- `ACCESS_FINE_LOCATION` - Required for BLE scanning
- `MODIFY_AUDIO_SETTINGS` - Audio configuration
- `FOREGROUND_SERVICE` - Background audio playback
- `FOREGROUND_SERVICE_MEDIA_PLAYBACK` - Media playback service

**Note**: The `react-native-ble-plx` plugin with `isBackgroundEnabled: true` automatically adds required BLE permissions to AndroidManifest.xml during the build process.

---

## Platform-Specific Setup

### iOS Setup

#### 1. Generate Native iOS Project (First Time Only)

```bash
npx expo prebuild --platform ios
```

This creates the `ios/` directory with Xcode project files.

#### 2. Install CocoaPods Dependencies

```bash
cd ios
pod install
cd ..
```

#### 3. Verify Background Modes

Open `ios/app/Info.plist` and verify these entries exist:

```xml
<key>UIBackgroundModes</key>
<array>
    <string>audio</string>
    <string>bluetooth-central</string>
</array>
```

#### 4. Code Signing (for Physical Devices)

1. Open `ios/app.xcworkspace` in Xcode
2. Select the project in the navigator
3. Go to "Signing & Capabilities"
4. Select your Team from the dropdown
5. Xcode will automatically manage provisioning

### Android Setup

#### 1. Generate Native Android Project (First Time Only)

```bash
npx expo prebuild --platform android
```

This creates the `android/` directory with Gradle project files.

#### 2. Verify Permissions

Open `android/app/src/main/AndroidManifest.xml` and verify these permissions exist:

```xml
<!-- BLE Permissions (automatically added by plugin) -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

<!-- Audio Permissions (automatically added by plugin) -->
<uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK" />

<!-- Feature declarations -->
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
```

**Note**: If permissions are missing after prebuild, they will be added automatically when you build the app.

#### 3. Runtime Permissions

Android 12+ requires runtime permission requests. The app automatically requests:
- Bluetooth Scan
- Bluetooth Connect
- Fine Location (for BLE scanning)

Users must grant these permissions when prompted.

---

## Building the App

### Development Builds

#### Start Development Server

```bash
npx expo start
```

Options:
- Press `i` - Open in iOS Simulator
- Press `a` - Open in Android Emulator
- Scan QR code with Expo Go app (limited BLE support)

**Important**: For full BLE functionality, use development builds on physical devices.

#### Create Development Build

**iOS**:
```bash
npx expo run:ios
```

**Android**:
```bash
npx expo run:android
```

### Production Builds

#### iOS Production Build

```bash
# Create production build
eas build --platform ios --profile production

# Or local build
npx expo run:ios --configuration Release
```

#### Android Production Build

```bash
# Create APK
eas build --platform android --profile production

# Or local build
npx expo run:android --variant release
```

### Build Profiles

Configure build profiles in `eas.json` (create if needed):

```json
{
  "build": {
    "development": {
      "developmentClient": true,
      "distribution": "internal"
    },
    "production": {
      "distribution": "store"
    }
  }
}
```

---

## Testing

### Testing Checklist

#### Initial Setup Tests
- [ ] App installs successfully
- [ ] App launches without crashes
- [ ] Permissions are requested correctly
- [ ] UI renders correctly in light/dark mode

#### Bluetooth Tests
- [ ] BLE scan discovers ESP32 device
- [ ] Can connect to ESP32 device
- [ ] Device info reads correctly
- [ ] Audio config reads correctly
- [ ] Connection remains stable

#### Audio Tests
- [ ] Audio stream starts
- [ ] Audio plays without stuttering
- [ ] Volume control works
- [ ] Buffer health stays > 0.1s
- [ ] No audio underruns under normal conditions
- [ ] Audio continues in background (iOS)

#### Control Tests
- [ ] START_STREAM command works
- [ ] STOP_STREAM command works
- [ ] SET_GAIN command works
- [ ] Commands receive responses

#### Error Handling Tests
- [ ] Handles device disconnection gracefully
- [ ] Shows appropriate error messages
- [ ] Recovers from connection failures
- [ ] Handles Bluetooth off state
- [ ] Handles missing permissions

### Testing on Physical Devices

#### iOS Device Testing

1. **Connect device via USB**
2. **Trust computer** on device when prompted
3. **Run app**:
   ```bash
   npx expo run:ios --device
   ```
4. **Grant permissions** when prompted

#### Android Device Testing

1. **Enable Developer Options**:
   - Go to Settings > About Phone
   - Tap "Build Number" 7 times

2. **Enable USB Debugging**:
   - Settings > Developer Options > USB Debugging

3. **Connect device via USB**

4. **Verify device connection**:
   ```bash
   adb devices
   ```

5. **Run app**:
   ```bash
   npx expo run:android
   ```

6. **Grant permissions** when prompted

### BLE Testing Tools

#### Using nRF Connect Mobile App

1. Install nRF Connect (iOS/Android)
2. Scan for "BabyMonitor-XXXX"
3. Connect and verify services:
   - Service UUID: `00001900-0000-1000-8000-00805F9B34FB`
   - Characteristics: Audio Data, Config, Control, Device Info
4. Enable notifications on Audio Data
5. Write control commands to test

#### Using Python Test Client

```bash
cd /home/bjorn/projects/babycall/esp
python3 test_ble_client.py
```

This script tests:
- BLE connection
- Reading device info
- Reading audio config
- Sending control commands
- Receiving audio data

---

## Troubleshooting

### Common Issues

#### BLE Scan Finds No Devices

**Symptoms**: Scan completes but no devices found

**Solutions**:
1. Ensure ESP32 is powered on and advertising
2. Check Bluetooth is enabled on phone
3. Grant Location permission (Android)
4. Restart the app
5. Check ESP32 logs for BLE initialization errors

**iOS Specific**:
```bash
# Check Bluetooth state in code
const state = await BLEService.manager?.state();
console.log('BLE State:', state); // Should be 'PoweredOn'
```

**Android Specific**:
- Android 12+: Ensure BLUETOOTH_SCAN and BLUETOOTH_CONNECT are granted
- Location services must be enabled system-wide
- Some Android devices require location permission for BLE

#### Connection Fails or Drops

**Symptoms**: Cannot connect or connection drops frequently

**Solutions**:
1. **Check distance**: Keep device within 10 meters of ESP32
2. **Reduce interference**: Move away from WiFi routers, microwaves
3. **Check ESP32 power**: Ensure adequate power supply (500mA+)
4. **Update firmware**: Ensure ESP32 firmware is latest version
5. **Check logs**: Monitor both app and ESP32 logs

**ESP32 Logs**:
```bash
cd /home/bjorn/projects/babycall/esp
idf.py monitor
```

Look for:
- `BLE initialized successfully`
- `Client connected`
- `MTU negotiated: XXX`

#### Audio Not Playing

**Symptoms**: Connected but no audio output

**Solutions**:
1. **Check audio config**:
   ```typescript
   const config = await BLEService.readAudioConfig();
   console.log('Sample Rate:', config.sampleRate);
   console.log('Status:', config.status); // Should be 1 (streaming)
   ```

2. **Verify audio stream started**:
   ```typescript
   await BLEService.sendControlCommand(ControlCommand.START_STREAM);
   await BLEService.startAudioStream(onAudioData);
   ```

3. **Check buffer health**:
   ```typescript
   const health = audioService.getBufferHealth();
   console.log('Buffer Health:', health); // Should be > 0
   ```

4. **Check volume**:
   ```typescript
   audioService.setVolume(1.0); // Max volume
   ```

5. **Monitor statistics**:
   ```typescript
   const stats = audioService.getStats();
   console.log('Frames Played:', stats.framesPlayed);
   console.log('Underruns:', stats.underruns);
   ```

#### Audio Stuttering or Dropouts

**Symptoms**: Audio plays but has gaps or stutters

**Solutions**:
1. **Check buffer health**: Should be 0.1-0.5 seconds
   ```typescript
   setInterval(() => {
     console.log('Buffer:', audioService.getBufferHealth());
   }, 1000);
   ```

2. **Monitor underruns**:
   ```typescript
   const stats = audioService.getStats();
   if (stats.underruns > 10) {
     console.warn('Too many underruns!');
   }
   ```

3. **Improve BLE connection**:
   - Move closer to ESP32
   - Reduce interference
   - Check MTU size: Higher is better (aim for 512)

4. **Check ESP32 streaming rate**:
   - Ensure consistent packet delivery
   - Monitor ESP32 CPU load

#### Permission Denied Errors

**iOS**:
1. Settings > App > Bluetooth > Enable
2. Settings > Privacy > Bluetooth > Enable for app
3. Delete and reinstall app if permissions stuck

**Android**:
1. Settings > Apps > Baby Monitor > Permissions
2. Enable: Bluetooth, Location, Nearby Devices
3. For Android 12+: "Nearby devices" = BLE permission

#### Build Errors

**iOS CocoaPods Errors**:
```bash
cd ios
pod repo update
pod deintegrate
pod install
cd ..
```

**Android Gradle Errors**:
```bash
cd android
./gradlew clean
cd ..
npx expo run:android
```

**Metro Bundler Cache Issues**:
```bash
npx expo start --clear
```

**Complete Clean Build**:
```bash
# Remove node_modules and lock file
rm -rf node_modules package-lock.json

# Remove build artifacts
rm -rf ios android .expo

# Reinstall
npm install
npx expo prebuild
```

#### Background Audio Not Working (iOS)

**Symptoms**: Audio stops when app goes to background

**Verify**:
1. Check Info.plist has `audio` background mode
2. Ensure AudioSession is configured:
   ```typescript
   // In react-native-audio-api, this is automatic
   // but verify in logs
   ```

3. Check Xcode capabilities:
   - Open `ios/app.xcworkspace`
   - Select target > Signing & Capabilities
   - Verify "Background Modes" capability exists
   - Ensure "Audio" is checked

---

## ESP32 Device Setup

### Prerequisites

- ESP-IDF v5.0 or later
- ESP32-C3 development board
- USB-C cable for programming
- Serial terminal (built into ESP-IDF)

### Enabling Bluetooth

**Before building the ESP32 firmware**, Bluetooth must be enabled:

#### Using menuconfig

```bash
cd /home/bjorn/projects/babycall/esp
idf.py menuconfig
```

**Required Settings**:

1. **Component config → Bluetooth**
   - `[*]` Bluetooth

2. **Bluetooth → Bluetooth Host**
   - Select: `Bluedroid - Dual-mode`

3. **Bluetooth → Controller Options**
   - `[*]` Bluetooth controller
   - `[*]` BLE support

4. **Bluetooth → Bluedroid Options**
   - `[*]` Gatt Module Enable
   - MTU Size: `512` (improves throughput)

5. **Save and Exit**
   - Press `S` to save
   - Press `Q` to quit

#### Using sdkconfig.defaults (Alternative)

Create or modify `/home/bjorn/projects/babycall/esp/sdkconfig.defaults`:

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

### Building and Flashing ESP32 Firmware

```bash
cd /home/bjorn/projects/babycall/esp

# Build firmware
idf.py build

# Flash to device (auto-detects USB port)
idf.py flash

# Open serial monitor to view logs
idf.py monitor

# Or combine flash + monitor
idf.py flash monitor
```

### Expected ESP32 Output

When working correctly, you should see:

```
I (123) BLE_AUDIO: BLE initialized successfully
I (456) BLE_AUDIO: Advertising started: BabyMonitor-XXXX
I (789) BLE_AUDIO: Client connected
I (890) BLE_AUDIO: MTU negotiated: 512
I (901) BLE_AUDIO: Audio streaming started
```

### ESP32 Troubleshooting

#### Bluetooth Initialization Failed

**Error**: `Failed to initialize BT controller`

**Solution**:
```bash
idf.py menuconfig
# Verify Bluetooth is enabled
# Save and rebuild
idf.py build flash
```

#### USB Port Not Found

**Error**: `Could not open port /dev/ttyUSB0`

**Solutions**:
1. Check USB connection
2. Install USB drivers (CH340/CP210x)
3. Add user to dialout group (Linux):
   ```bash
   sudo usermod -a -G dialout $USER
   # Logout and login
   ```
4. Specify port manually:
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```

#### Memory/Heap Errors

**Error**: `No resources` or heap allocation failures

**Solution**: Increase heap in menuconfig:
- Component config → ESP32-specific
- Main task stack size: 4096 or higher

### ESP32 BLE Service Details

**Service UUID**: `00001900-0000-1000-8000-00805F9B34FB`

**Characteristics**:
- **0x1901** - Audio Data (NOTIFY): PCM audio packets
- **0x1902** - Audio Config (READ): Sample rate, bit depth, channels
- **0x1903** - Control (WRITE): START/STOP/SET_GAIN commands
- **0x1904** - Device Info (READ): Firmware version, battery, uptime

**Audio Format**:
- Sample Rate: 16 kHz
- Bit Depth: 16-bit signed PCM
- Channels: Mono
- Packet Size: Up to MTU-3 bytes (~509 bytes with MTU=512)

For complete ESP32 documentation, see:
- `/home/bjorn/projects/babycall/esp/BLE_SETUP.md`
- `/home/bjorn/projects/babycall/esp/BLE_API_REFERENCE.md`

---

## Development Workflow

### Recommended Development Flow

1. **Start ESP32**:
   ```bash
   cd /home/bjorn/projects/babycall/esp
   idf.py monitor
   ```

2. **Start App** (separate terminal):
   ```bash
   cd /home/bjorn/projects/babycall/app
   npx expo start
   ```

3. **Deploy to Device**:
   - iOS: Press `i` or `npx expo run:ios --device`
   - Android: Press `a` or `npx expo run:android`

4. **Monitor Logs**:
   - App logs: Metro bundler console
   - ESP32 logs: `idf.py monitor` terminal
   - Device logs: Xcode console (iOS) or `adb logcat` (Android)

### Useful Commands

```bash
# View Android logs
adb logcat | grep "BLE\|Audio"

# View iOS logs (via Xcode)
# Xcode > Window > Devices and Simulators > Select device > View Device Logs

# Clear Metro cache
npx expo start --clear

# Reset Expo
npx expo start --reset-cache

# Check dependencies
npm outdated
```

---

## Additional Resources

### App Documentation

- `/home/bjorn/projects/babycall/app/README.md` - Expo app basics
- `/home/bjorn/projects/babycall/app/src/services/README.md` - AudioService docs
- `/home/bjorn/projects/babycall/app/src/services/BLEService.README.md` - BLE service docs
- `/home/bjorn/projects/babycall/app/src/services/QUICK_REFERENCE.md` - Quick reference
- `/home/bjorn/projects/babycall/AUDIO_SERVICE_IMPLEMENTATION.md` - Audio implementation

### ESP32 Documentation

- `/home/bjorn/projects/babycall/esp/BLE_SETUP.md` - BLE setup guide
- `/home/bjorn/projects/babycall/esp/BLE_API_REFERENCE.md` - API reference

### External Resources

- [Expo Documentation](https://docs.expo.dev/)
- [React Native Docs](https://reactnative.dev/)
- [react-native-ble-plx](https://github.com/dotintent/react-native-ble-plx)
- [react-native-audio-api](https://github.com/software-mansion/react-native-audio-api)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Bluetooth SIG](https://www.bluetooth.com/)

---

## Version Information

- **Expo**: 54.0.31
- **React Native**: 0.81.5
- **React**: 19.1.0
- **react-native-ble-plx**: 3.2.2
- **react-native-audio-api**: 0.3.7
- **ESP-IDF**: v5.0+ recommended
- **iOS**: 13.4+ required
- **Android**: API 31+ (Android 12+) required

---

## Support

For issues or questions:
1. Check this documentation first
2. Review service-specific documentation
3. Check ESP32 logs for device-side issues
4. Check app logs for client-side issues
5. Consult external documentation links above

## License

[Add your license information here]
