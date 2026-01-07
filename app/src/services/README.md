# AudioService

PCM audio playback service for React Native using `react-native-audio-api`.

## Features

- 16-bit PCM to Float32 conversion
- Continuous buffering with Web Audio API scheduling pattern
- Buffer underrun detection and logging
- Volume control (0.0 to 1.0)
- Playback statistics tracking
- Automatic context state management

## Usage

### Basic Setup

```typescript
import audioService from './services/AudioService';

// Initialize with default 16kHz sample rate
audioService.init(16000);

// Set volume (optional)
audioService.setVolume(0.8);
```

### Playing Audio

```typescript
// Receive Int16Array PCM samples (e.g., from BLE or WebSocket)
const samples: Int16Array = new Int16Array([...]);

// Play the frame - will be automatically scheduled
audioService.playPCMFrame(samples);
```

### Monitoring

```typescript
// Check buffer health (seconds of buffered audio)
const bufferSeconds = audioService.getBufferHealth();
console.log(`Buffer: ${bufferSeconds.toFixed(3)}s`);

// Get statistics
const stats = audioService.getStats();
console.log(`Frames: ${stats.framesPlayed}, Underruns: ${stats.underruns}`);

// Check playback status
if (audioService.isPlaying()) {
  console.log('Audio is playing');
}
```

### Stopping and Cleanup

```typescript
// Stop playback (keeps context initialized)
audioService.stop();

// Dispose resources (when completely done)
audioService.dispose();
```

## Implementation Details

### Scheduling Pattern

The service uses the same scheduling pattern as the WiFi example web client:

```javascript
// From example/audio_stream.c (JavaScript embedded in HTML)
const now = ctx.currentTime;
if (nextTime < now) nextTime = now;  // Detect underrun
src.start(nextTime);
nextTime += buf.duration;
```

This ensures:
- Continuous playback without gaps
- Automatic detection of buffer underruns
- Proper handling of timing issues

### Audio Format

- **Input**: Int16Array (16-bit signed PCM)
- **Conversion**: Divide by 32768.0 to get Float32 (-1.0 to 1.0)
- **Channels**: Mono (1 channel)
- **Sample Rate**: Configurable (default 16000 Hz)

### Error Handling

- Context initialization failures are thrown
- Playback errors are logged but don't stop the service
- Context state (suspended/running) is automatically managed
- Buffer underruns are detected and logged with statistics

## Statistics

The `getStats()` method returns:

```typescript
interface AudioStats {
  framesPlayed: number;    // Total frames played since init
  underruns: number;       // Number of buffer underruns
  bufferHealth: number;    // Current buffer (seconds)
  sampleRate: number;      // Configured sample rate
}
```

## Example: BLE Audio Stream

```typescript
import audioService from './services/AudioService';

// Initialize
audioService.init(16000);

// Handle incoming BLE audio packets
function onAudioData(data: ArrayBuffer) {
  const samples = new Int16Array(data);
  audioService.playPCMFrame(samples);

  // Log buffer health periodically
  const health = audioService.getBufferHealth();
  if (health < 0.1) {
    console.warn('Low buffer!');
  }
}

// Cleanup on disconnect
function onDisconnect() {
  audioService.stop();
  const stats = audioService.getStats();
  console.log('Playback stats:', stats);
}
```

## Notes

- The service exports a singleton instance by default
- You can also instantiate `new AudioService()` if needed
- Buffer underruns indicate the audio source isn't keeping up
- Recommended buffer health: 0.1 - 0.5 seconds
