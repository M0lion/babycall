# Baby Monitor App - Configuration Summary

This document provides a quick reference for all app configurations, permissions, and plugins.

## App Information

- **Name**: Baby Monitor
- **Version**: 1.0.0
- **iOS Bundle ID**: com.babycall.monitor
- **Android Package**: com.babycall.monitor

## Expo Configuration

- **Expo SDK**: 54.0.31
- **React Native**: 0.81.5
- **New Architecture**: Enabled
- **Typed Routes**: Enabled (experimental)
- **React Compiler**: Enabled (experimental)

---

## iOS Configuration

### Bundle Identifier
```
com.babycall.monitor
```

### Background Modes
✅ Configured in `app.json` → `ios.infoPlist.UIBackgroundModes`:
- `audio` - Background audio playback
- `bluetooth-central` - Background BLE communication

### Permission Descriptions (Info.plist)
✅ All required descriptions configured:

| Permission Key | Description | Purpose |
|---------------|-------------|---------|
| NSBluetoothAlwaysUsageDescription | "Connect to baby monitor device for audio streaming" | BLE access |
| NSBluetoothPeripheralUsageDescription | "Connect to baby monitor device for audio streaming" | BLE peripheral mode |
| NSMicrophoneUsageDescription | "Access microphone to monitor audio from baby monitor device" | Audio monitoring |

### iOS Capabilities Required
When building in Xcode, ensure these are enabled:
- ✅ Background Modes capability
  - Audio, AirPlay, and Picture in Picture
  - Acts as a Bluetooth LE accessory

---

## Android Configuration

### Package Name
```
com.babycall.monitor
```

### Permissions
✅ Configured in `app.json` → `android.permissions`:

| Permission | API Level | Purpose |
|-----------|-----------|---------|
| android.permission.BLUETOOTH_SCAN | 31+ (Android 12+) | Scan for BLE devices |
| android.permission.BLUETOOTH_CONNECT | 31+ (Android 12+) | Connect to BLE devices |
| android.permission.ACCESS_FINE_LOCATION | All | Required for BLE scanning |
| android.permission.MODIFY_AUDIO_SETTINGS | All | Audio configuration |
| android.permission.FOREGROUND_SERVICE | 26+ (Android 8+) | Background service |
| android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK | 34+ (Android 14+) | Media playback service |

### Hardware Features
The following will be added to AndroidManifest.xml during build:
```xml
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
```

### Runtime Permissions
Users will be prompted for these permissions at runtime:
- Bluetooth Scan (Android 12+)
- Bluetooth Connect (Android 12+)
- Fine Location (all versions)

---

## Plugin Configuration

### 1. react-native-ble-plx (v3.2.2)

**Purpose**: Bluetooth Low Energy communication

**Configuration**:
```json
{
  "isBackgroundEnabled": true,
  "modes": ["peripheral", "central"],
  "bluetoothAlwaysUsageDescription": "Connect to baby monitor device for audio streaming"
}
```

**Features Enabled**:
- ✅ Background BLE operation
- ✅ Central mode (connect to ESP32)
- ✅ Peripheral mode (future features)
- ✅ Automatic permission injection

**What This Plugin Adds**:
- iOS: Background bluetooth-central capability
- Android: BLUETOOTH_SCAN, BLUETOOTH_CONNECT, ACCESS_FINE_LOCATION permissions
- Both: BLE manager initialization code

### 2. react-native-audio-api (v0.3.7)

**Purpose**: Audio playback with Web Audio API

**Configuration**:
```json
{
  "iosBackgroundMode": true,
  "androidForegroundService": true,
  "androidFSTypes": ["mediaPlayback"],
  "androidPermissions": [
    "android.permission.MODIFY_AUDIO_SETTINGS",
    "android.permission.FOREGROUND_SERVICE",
    "android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK"
  ]
}
```

**Features Enabled**:
- ✅ iOS background audio playback
- ✅ Android foreground service for media
- ✅ Audio settings modification
- ✅ Continuous audio streaming

**What This Plugin Adds**:
- iOS: Background audio capability
- Android: Foreground service with media playback type
- Both: AudioContext and Web Audio API implementation

### 3. expo-router (v6.0.21)

**Purpose**: File-based routing

**Configuration**: Default

**Features**:
- File-based navigation
- Deep linking support
- Typed routes (experimental)

### 4. expo-splash-screen (v31.0.13)

**Purpose**: Custom splash screen

**Configuration**:
```json
{
  "image": "./assets/images/splash-icon.png",
  "imageWidth": 200,
  "resizeMode": "contain",
  "backgroundColor": "#ffffff",
  "dark": {
    "backgroundColor": "#000000"
  }
}
```

---

## Required Dependencies

### Core Dependencies
```json
{
  "expo": "^54.0.31",
  "react": "19.1.0",
  "react-native": "0.81.5",
  "react-native-ble-plx": "^3.2.2",
  "react-native-audio-api": "^0.3.7",
  "expo-router": "~6.0.21"
}
```

### Native Module Dependencies
These require native builds (cannot use Expo Go):
- ✅ react-native-ble-plx
- ✅ react-native-audio-api

**Important**: You must use `npx expo run:ios` or `npx expo run:android` for development with these modules.

---

## Verification Checklist

### After Initial Setup
- [ ] Run `npm install` successfully
- [ ] No dependency conflicts
- [ ] TypeScript compiles without errors

### Before Building
- [ ] iOS bundle identifier set: `com.babycall.monitor`
- [ ] Android package name set: `com.babycall.monitor`
- [ ] All permissions configured in `app.json`
- [ ] Plugin configurations verified

### iOS Build Verification
- [ ] Run `npx expo prebuild --platform ios`
- [ ] Check `ios/BabyMonitor/Info.plist` contains:
  - NSBluetoothAlwaysUsageDescription
  - NSBluetoothPeripheralUsageDescription
  - NSMicrophoneUsageDescription
  - UIBackgroundModes with audio and bluetooth-central
- [ ] Open in Xcode and verify capabilities
- [ ] Code signing configured

### Android Build Verification
- [ ] Run `npx expo prebuild --platform android`
- [ ] Check `android/app/src/main/AndroidManifest.xml` contains:
  - BLUETOOTH_SCAN permission
  - BLUETOOTH_CONNECT permission
  - ACCESS_FINE_LOCATION permission
  - MODIFY_AUDIO_SETTINGS permission
  - FOREGROUND_SERVICE permissions
  - bluetooth_le feature declaration
- [ ] Gradle sync successful

### Runtime Verification
- [ ] App requests permissions on first launch
- [ ] BLE scan works
- [ ] BLE connection succeeds
- [ ] Audio plays in foreground
- [ ] Audio continues in background (iOS)
- [ ] No permission errors in logs

---

## Configuration Files Reference

### Primary Configuration
- `/home/bjorn/projects/babycall/app/app.json` - Main Expo configuration

### Generated After Prebuild
- `/home/bjorn/projects/babycall/app/ios/BabyMonitor/Info.plist` - iOS permissions
- `/home/bjorn/projects/babycall/app/android/app/src/main/AndroidManifest.xml` - Android permissions

### Package Configuration
- `/home/bjorn/projects/babycall/app/package.json` - Dependencies
- `/home/bjorn/projects/babycall/app/tsconfig.json` - TypeScript config

---

## Common Configuration Issues

### Issue: Permissions Not Added After Prebuild

**Solution**:
```bash
# Clean and rebuild
rm -rf ios android
npx expo prebuild
```

### Issue: iOS Background Audio Not Working

**Check**:
1. Info.plist has `audio` in UIBackgroundModes
2. AudioSession is active
3. Audio is actually playing when backgrounded

### Issue: Android BLE Permissions Denied

**Check**:
1. Location services enabled (system-wide)
2. App has location permission
3. Bluetooth is enabled
4. Android 12+: "Nearby devices" permission granted

### Issue: Plugin Not Found After Install

**Solution**:
```bash
npm install
npx expo prebuild --clean
```

---

## ESP32 BLE Service Configuration

The app expects the ESP32 to expose these BLE characteristics:

### Service UUID
```
00001900-0000-1000-8000-00805F9B34FB
```

### Characteristics
- **0x1901** - Audio Data (NOTIFY)
- **0x1902** - Audio Config (READ, WRITE)
- **0x1903** - Control Command (WRITE, NOTIFY)
- **0x1904** - Device Info (READ)

### Expected Device Name Pattern
```
BabyMonitor-XXXX
```
Where XXXX is derived from MAC address or device ID.

---

## Additional Documentation

For detailed setup instructions, see:
- `/home/bjorn/projects/babycall/app/SETUP.md` - Complete setup guide
- `/home/bjorn/projects/babycall/app/src/services/BLEService.README.md` - BLE service documentation
- `/home/bjorn/projects/babycall/app/src/services/README.md` - Audio service documentation

---

## Version History

### v1.0.0 (Current)
- Initial release
- BLE audio streaming support
- iOS and Android support
- Background audio playback
- ESP32-C3 device support

---

## Status: Configuration Complete ✅

All plugins and permissions are properly configured for:
- ✅ Bluetooth Low Energy (background-enabled)
- ✅ Audio playback (background-enabled on iOS)
- ✅ iOS background modes
- ✅ Android foreground service
- ✅ All required permissions

The app is ready to build and deploy.
