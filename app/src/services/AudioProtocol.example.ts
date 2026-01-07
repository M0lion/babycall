/**
 * @file AudioProtocol.example.ts
 * @brief Example usage of the AudioProtocol class
 *
 * This file demonstrates how to use the AudioProtocol class
 * to process incoming BLE audio packets from the ESP32 baby monitor.
 */

import { AudioProtocol } from './AudioProtocol';

// Example 1: Using the singleton instance
import audioProtocol from './AudioProtocol';

/**
 * Example: Processing incoming BLE data
 */
function processBLEData(blePacket: Uint8Array): void {
  // Process the incoming packet
  const audioFrame = audioProtocol.processIncomingData(blePacket);

  if (audioFrame) {
    // Complete frame received - ready for playback
    console.log(`Received complete audio frame: ${audioFrame.length} samples`);

    // Pass to audio playback system
    playAudioFrame(audioFrame);
  }
  // If null, frame is still being assembled (fragmented packets)
}

/**
 * Example: Manual packet parsing and reassembly
 */
function manualProcessing(blePacket: Uint8Array): void {
  // Step 1: Parse the packet
  const packet = audioProtocol.parsePacket(blePacket);

  if (!packet) {
    console.error('Failed to parse packet');
    return;
  }

  console.log(`Parsed packet - Sequence: ${packet.sequence}, Fragment: ${packet.fragmentIndex}/${packet.isLast ? 'last' : 'more'}`);

  // Step 2: Reassemble frame
  const audioFrame = audioProtocol.reassembleFrame(packet);

  if (audioFrame) {
    console.log(`Frame complete: ${audioFrame.length} samples`);
    playAudioFrame(audioFrame);
  }
}

/**
 * Example: Monitoring statistics
 */
function monitorStatistics(): void {
  const stats = audioProtocol.getStats();

  console.log('Audio Protocol Statistics:');
  console.log(`  Total Packets: ${stats.totalPackets}`);
  console.log(`  Total Frames: ${stats.totalFrames}`);
  console.log(`  Dropped Frames: ${stats.droppedFrames}`);
  console.log(`  Packet Loss Rate: ${(stats.packetLossRate * 100).toFixed(2)}%`);
  console.log(`  Last Sequence: ${stats.lastSequence}`);
}

/**
 * Example: Resetting protocol state (e.g., on reconnection)
 */
function handleBLEReconnect(): void {
  console.log('BLE reconnected - resetting protocol state');
  audioProtocol.reset();
}

/**
 * Example: Creating a custom instance (if needed)
 */
function customInstance(): void {
  const customProtocol = new AudioProtocol();

  // Use custom instance
  const frame = customProtocol.processIncomingData(new Uint8Array());

  // Get its own stats
  const stats = customProtocol.getStats();
  console.log(stats);
}

/**
 * Example: Integration with React Native BLE library
 */
async function bleIntegrationExample(device: any): Promise<void> {
  // Assuming using react-native-ble-plx

  // Subscribe to audio data characteristic
  device.monitorCharacteristicForService(
    'AUDIO_SERVICE_UUID',
    'AUDIO_DATA_CHARACTERISTIC_UUID',
    (error: any, characteristic: any) => {
      if (error) {
        console.error('BLE Error:', error);
        return;
      }

      if (characteristic?.value) {
        // Decode base64 to Uint8Array
        const base64Data = characteristic.value;
        const binaryData = base64ToUint8Array(base64Data);

        // Process with AudioProtocol
        const audioFrame = audioProtocol.processIncomingData(binaryData);

        if (audioFrame) {
          playAudioFrame(audioFrame);
        }
      }
    }
  );
}

/**
 * Placeholder function - implement actual audio playback
 */
function playAudioFrame(audioData: Int16Array): void {
  // TODO: Implement audio playback using react-native-audio-api
  // Example: Convert Int16Array to Float32Array and queue for playback
  console.log(`Playing ${audioData.length} audio samples`);
}

/**
 * Helper: Convert base64 to Uint8Array
 */
function base64ToUint8Array(base64: string): Uint8Array {
  const binaryString = atob(base64);
  const len = binaryString.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes;
}

export {
  processBLEData,
  manualProcessing,
  monitorStatistics,
  handleBLEReconnect,
  customInstance,
  bleIntegrationExample,
};
