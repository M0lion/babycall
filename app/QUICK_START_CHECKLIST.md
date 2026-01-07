# Baby Monitor App - Quick Start Checklist

Quick reference for setting up and verifying the baby monitor app configuration.

## 1. Initial Setup (5 minutes)

```bash
cd /home/bjorn/projects/babycall/app

# Install dependencies
npm install

# Verify installation
npx expo --version
```

**Expected Output**: `54.0.31` or similar

---

## 2. Configuration Verification (2 minutes)

### Check app.json
```bash
cat app.json | grep -E "(bundleIdentifier|package|BLUETOOTH|AUDIO|background)" | head -20
```

**Should see**:
- ✅ iOS bundleIdentifier: com.babycall.monitor
- ✅ Android package: com.babycall.monitor
- ✅ BLUETOOTH_SCAN, BLUETOOTH_CONNECT
- ✅ UIBackgroundModes: audio, bluetooth-central

---

## 3. Generate Native Projects (5 minutes)

### For iOS Development
```bash
npx expo prebuild --platform ios
cd ios && pod install && cd ..
```

### For Android Development
```bash
npx expo prebuild --platform android
```

### For Both Platforms
```bash
npx expo prebuild
```

---

## 4. Quick Verification Tests

### Test 1: Check iOS Permissions
```bash
grep -A 10 "UIBackgroundModes" ios/*/Info.plist
```
**Should show**: `audio` and `bluetooth-central`

### Test 2: Check Android Permissions
```bash
grep "BLUETOOTH\|FOREGROUND" android/app/src/main/AndroidManifest.xml
```
**Should show**: 6 permissions including BLUETOOTH_SCAN, BLUETOOTH_CONNECT

### Test 3: Validate JSON
```bash
cat app.json | python3 -m json.tool > /dev/null && echo "✓ Valid" || echo "✗ Invalid"
```
**Should show**: ✓ Valid

---

## 5. Build and Run (10 minutes)

### iOS (Physical Device Recommended)
```bash
# Connect iOS device via USB
npx expo run:ios --device
```

### Android (Physical Device Recommended)
```bash
# Enable USB debugging on Android device
# Connect via USB
npx expo run:android
```

### Start Development Server Only
```bash
npx expo start
# Then press 'i' for iOS or 'a' for Android
```

---

## 6. ESP32 Setup (10 minutes)

```bash
cd /home/bjorn/projects/babycall/esp

# Configure Bluetooth
idf.py menuconfig
# Enable: Component config → Bluetooth
# Set MTU: Bluetooth → Bluedroid Options → MTU Size: 512

# Build and flash
idf.py build flash monitor
```

**Expected Output**: `BLE initialized successfully`, `Advertising started`

---

## 7. First Run Checklist

When app launches for the first time:

### iOS
- [ ] App launches successfully
- [ ] Bluetooth permission request appears
- [ ] Grant Bluetooth permission
- [ ] App UI loads correctly

### Android
- [ ] App launches successfully
- [ ] "Nearby devices" permission request appears
- [ ] Grant Nearby devices permission
- [ ] Location permission request appears
- [ ] Grant Location permission
- [ ] App UI loads correctly

---

## 8. Connection Test (5 minutes)

### In the App:
1. [ ] Tap "Scan for Devices"
2. [ ] See "BabyMonitor-XXXX" in device list
3. [ ] Tap to connect
4. [ ] Connection successful
5. [ ] See device info (firmware version, battery)
6. [ ] Tap "Start Monitoring"
7. [ ] Hear audio from ESP32 device
8. [ ] Audio continues in background (iOS)

### If No Devices Found:
- [ ] Check ESP32 is powered on
- [ ] Check Bluetooth is enabled on phone
- [ ] Check Location services enabled (Android)
- [ ] Check ESP32 logs: `idf.py monitor`
- [ ] Try moving closer to ESP32

---

## 9. Audio Playback Test (3 minutes)

### In the App:
1. [ ] Start audio stream
2. [ ] Check audio plays smoothly
3. [ ] Adjust volume (should work)
4. [ ] Check buffer health > 0.1s
5. [ ] Background app (iOS) - audio continues
6. [ ] Foreground app - audio still playing
7. [ ] Stop stream - audio stops

### If Audio Doesn't Play:
- [ ] Check volume is not muted
- [ ] Check audio stream started (send START_STREAM)
- [ ] Check buffer health (should be > 0)
- [ ] Check ESP32 is streaming (monitor logs)
- [ ] Check for underruns in app logs

---

## 10. Troubleshooting Commands

### Clear Everything and Rebuild
```bash
cd /home/bjorn/projects/babycall/app
rm -rf node_modules ios android .expo package-lock.json
npm install
npx expo prebuild
```

### View iOS Device Logs
```bash
# In Xcode: Window → Devices and Simulators → Select Device → Open Console
```

### View Android Device Logs
```bash
adb logcat | grep -E "BLE|Audio|Baby"
```

### ESP32 Logs
```bash
cd /home/bjorn/projects/babycall/esp
idf.py monitor
```

### Clear Metro Cache
```bash
npx expo start --clear
```

---

## Configuration File Locations

| File | Purpose |
|------|---------|
| `/home/bjorn/projects/babycall/app/app.json` | Main configuration |
| `/home/bjorn/projects/babycall/app/package.json` | Dependencies |
| `/home/bjorn/projects/babycall/app/SETUP.md` | Full setup guide |
| `/home/bjorn/projects/babycall/app/CONFIGURATION_SUMMARY.md` | Config reference |
| `/home/bjorn/projects/babycall/app/CONFIGURATION_VERIFICATION.md` | Verification report |

---

## Quick Reference: Required Versions

| Component | Version | Status |
|-----------|---------|--------|
| Node.js | 18.x+ | ✅ |
| Expo | 54.0.31 | ✅ |
| React Native | 0.81.5 | ✅ |
| react-native-ble-plx | 3.2.2 | ✅ |
| react-native-audio-api | 0.3.7 | ✅ |
| iOS | 13.4+ | Required |
| Android | API 31+ | Required |
| ESP-IDF | 5.0+ | Required |

---

## Quick Reference: Permissions

### iOS
- ✅ NSBluetoothAlwaysUsageDescription
- ✅ NSBluetoothPeripheralUsageDescription
- ✅ NSMicrophoneUsageDescription
- ✅ UIBackgroundModes: audio, bluetooth-central

### Android
- ✅ BLUETOOTH_SCAN
- ✅ BLUETOOTH_CONNECT
- ✅ ACCESS_FINE_LOCATION
- ✅ MODIFY_AUDIO_SETTINGS
- ✅ FOREGROUND_SERVICE
- ✅ FOREGROUND_SERVICE_MEDIA_PLAYBACK

---

## Quick Reference: BLE Service

| UUID | Name | Properties |
|------|------|-----------|
| 0x1900 | Baby Monitor Service | - |
| 0x1901 | Audio Data | NOTIFY |
| 0x1902 | Audio Config | READ, WRITE |
| 0x1903 | Control Command | WRITE, NOTIFY |
| 0x1904 | Device Info | READ |

**Full UUIDs**: `00001900-0000-1000-8000-00805F9B34FB` (add 01-04 for characteristics)

---

## Success Criteria

Your setup is successful when:
- ✅ App builds without errors
- ✅ App launches on device
- ✅ Permissions granted
- ✅ BLE scan finds ESP32
- ✅ Connection succeeds
- ✅ Audio plays smoothly
- ✅ Background audio works (iOS)
- ✅ No crashes or errors

---

## Need Help?

1. **For setup issues**: See `/home/bjorn/projects/babycall/app/SETUP.md`
2. **For BLE issues**: See `/home/bjorn/projects/babycall/app/src/services/BLEService.README.md`
3. **For audio issues**: See `/home/bjorn/projects/babycall/app/src/services/README.md`
4. **For ESP32 issues**: See `/home/bjorn/projects/babycall/esp/BLE_SETUP.md`

---

**Status**: Configuration complete and verified ✅
**Ready to build**: Yes ✅
**Documentation**: Complete ✅
