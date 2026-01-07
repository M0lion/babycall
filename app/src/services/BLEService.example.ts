/**
 * BLEService Usage Examples
 *
 * This file demonstrates how to use the BLEService to connect to
 * the ESP32 baby monitor device and interact with it.
 */

import BLEService from './BLEService';
import { Device, ControlCommand, AudioConfig, DeviceInfo } from '../types';

/**
 * Example 1: Initialize and request permissions
 */
async function initializeExample() {
  // Initialize the BLE manager
  BLEService.init();

  // Request permissions (required on Android)
  const granted = await BLEService.requestPermissions();
  if (!granted) {
    console.error('Bluetooth permissions not granted');
    return;
  }

  console.log('BLE initialized and permissions granted');
}

/**
 * Example 2: Scan for baby monitor devices
 */
async function scanExample() {
  const devices: Device[] = [];

  await BLEService.scanForDevices(
    (device) => {
      console.log('Found device:', device.name || device.id);
      devices.push(device);
    },
    10000 // Scan for 10 seconds
  );

  // After timeout, stop scanning
  console.log(`Scan complete. Found ${devices.length} devices`);
  return devices;
}

/**
 * Example 3: Connect to a device
 */
async function connectExample(device: Device) {
  try {
    // Stop scanning before connecting
    BLEService.stopScan();

    // Connect to the device
    await BLEService.connect(device);
    console.log('Connected successfully');

    // Setup disconnect callback
    BLEService.onDisconnect(() => {
      console.log('Device disconnected unexpectedly!');
    });

    return true;
  } catch (error) {
    console.error('Connection failed:', error);
    return false;
  }
}

/**
 * Example 4: Read device information
 */
async function readDeviceInfoExample() {
  try {
    const info: DeviceInfo = await BLEService.readDeviceInfo();

    // Parse firmware version (e.g., 0x00010203 = v1.2.3)
    const major = (info.firmwareVersion >> 24) & 0xFF;
    const minor = (info.firmwareVersion >> 16) & 0xFF;
    const patch = (info.firmwareVersion >> 8) & 0xFF;

    console.log(`Firmware: v${major}.${minor}.${patch}`);
    console.log(`Battery: ${info.batteryLevel}%`);
    console.log(`Uptime: ${info.uptime} seconds`);

    return info;
  } catch (error) {
    console.error('Failed to read device info:', error);
  }
}

/**
 * Example 5: Read audio configuration
 */
async function readAudioConfigExample() {
  try {
    const config: AudioConfig = await BLEService.readAudioConfig();

    console.log(`Sample Rate: ${config.sampleRate} Hz`);
    console.log(`Bit Depth: ${config.bitDepth} bits`);
    console.log(`Channels: ${config.channels === 1 ? 'Mono' : 'Stereo'}`);
    console.log(`Status: ${config.status === 1 ? 'Streaming' : 'Stopped'}`);

    return config;
  } catch (error) {
    console.error('Failed to read audio config:', error);
  }
}

/**
 * Example 6: Start audio streaming
 */
async function startStreamingExample() {
  try {
    // Send START_STREAM command
    await BLEService.sendControlCommand(ControlCommand.START_STREAM);
    console.log('Start stream command sent');

    // Start listening for audio data
    await BLEService.startAudioStream((audioData) => {
      // Audio data format: [version|sequence|fragment|length|audio_data]
      const version = audioData[0];
      const sequence = (audioData[1] | (audioData[2] << 8));
      const fragment = audioData[3];
      const length = (audioData[4] | (audioData[5] << 8));
      const audio = audioData.slice(6, 6 + length);

      console.log(`Received audio packet: seq=${sequence}, frag=${fragment}, len=${length}`);

      // Process audio data here
      // - Reassemble fragments
      // - Decode audio
      // - Play audio
    });

    console.log('Audio stream started');
  } catch (error) {
    console.error('Failed to start streaming:', error);
  }
}

/**
 * Example 7: Stop audio streaming
 */
async function stopStreamingExample() {
  try {
    // Stop listening for audio data
    BLEService.stopAudioStream();

    // Send STOP_STREAM command
    await BLEService.sendControlCommand(ControlCommand.STOP_STREAM);
    console.log('Streaming stopped');
  } catch (error) {
    console.error('Failed to stop streaming:', error);
  }
}

/**
 * Example 8: Adjust gain
 */
async function setGainExample(gainValue: number) {
  try {
    // Gain value as parameter (e.g., 0-255)
    const params = new Uint8Array([gainValue]);
    await BLEService.sendControlCommand(ControlCommand.SET_GAIN, params);
    console.log(`Gain set to ${gainValue}`);
  } catch (error) {
    console.error('Failed to set gain:', error);
  }
}

/**
 * Example 9: Complete workflow
 */
async function completeWorkflowExample() {
  try {
    // 1. Initialize
    BLEService.init();
    const granted = await BLEService.requestPermissions();
    if (!granted) {
      throw new Error('Permissions not granted');
    }

    // 2. Scan for devices
    let selectedDevice: Device | null = null;
    await BLEService.scanForDevices((device) => {
      console.log('Found:', device.name || device.id);
      if (!selectedDevice) {
        selectedDevice = device;
        BLEService.stopScan(); // Stop after finding first device
      }
    }, 10000);

    if (!selectedDevice) {
      throw new Error('No devices found');
    }

    // 3. Connect
    await BLEService.connect(selectedDevice);
    console.log('Connected');

    // 4. Read device info
    const info = await BLEService.readDeviceInfo();
    console.log('Device info:', info);

    // 5. Read audio config
    const config = await BLEService.readAudioConfig();
    console.log('Audio config:', config);

    // 6. Start streaming
    await BLEService.sendControlCommand(ControlCommand.START_STREAM);
    await BLEService.startAudioStream((audioData) => {
      console.log('Audio data received:', audioData.length, 'bytes');
      // Process audio...
    });

    // 7. Wait for some time...
    await new Promise(resolve => setTimeout(resolve, 30000)); // 30 seconds

    // 8. Stop streaming
    BLEService.stopAudioStream();
    await BLEService.sendControlCommand(ControlCommand.STOP_STREAM);

    // 9. Disconnect
    await BLEService.disconnect();
    console.log('Workflow complete');
  } catch (error) {
    console.error('Workflow error:', error);
  }
}

/**
 * Example 10: Cleanup
 */
function cleanupExample() {
  // Destroy the BLE manager and clean up all resources
  BLEService.destroy();
  console.log('BLE Service cleaned up');
}

// Export examples
export {
  initializeExample,
  scanExample,
  connectExample,
  readDeviceInfoExample,
  readAudioConfigExample,
  startStreamingExample,
  stopStreamingExample,
  setGainExample,
  completeWorkflowExample,
  cleanupExample,
};
