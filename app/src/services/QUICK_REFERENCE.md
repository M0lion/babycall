# AudioService Quick Reference

## Import
```typescript
import audioService from './services/AudioService';
```

## Setup
```typescript
audioService.init(16000);        // Initialize with 16kHz
audioService.setVolume(0.8);     // Set volume 0.0-1.0
```

## Play Audio
```typescript
const samples = new Int16Array([...]); // Your PCM data
audioService.playPCMFrame(samples);    // Play the frame
```

## Monitor
```typescript
// Check buffer health
const bufferSeconds = audioService.getBufferHealth();

// Get statistics
const stats = audioService.getStats();
// stats = { framesPlayed, underruns, bufferHealth, sampleRate }

// Check if playing
const isPlaying = audioService.isPlaying();
```

## Control
```typescript
audioService.stop();         // Stop playback
audioService.dispose();      // Clean up resources
audioService.resetStats();   // Reset statistics
```

## Full Example
```typescript
// Initialize
audioService.init(16000);

// Play audio from BLE/WebSocket
function onAudioPacket(data: ArrayBuffer) {
  const pcm = new Int16Array(data);
  audioService.playPCMFrame(pcm);

  // Check buffer
  if (audioService.getBufferHealth() < 0.1) {
    console.warn('Low buffer!');
  }
}

// Cleanup
function onDisconnect() {
  console.log('Stats:', audioService.getStats());
  audioService.stop();
}
```

## Key Points
- Input: Int16Array (16-bit signed PCM)
- Automatic scheduling (no gaps)
- Detects buffer underruns
- Thread-safe (AudioContext handles threading)
- Singleton pattern (shared instance)
