/**
 * Centralized Audio Configuration
 *
 * This module provides all audio-related configuration settings
 * for the baby monitor application. Settings can be changed at
 * runtime through the BLEContext.
 */

export interface AudioSettings {
  sampleRate: number;
  bitDepth: number;
  channels: number;
  bufferSize?: number;
  gain?: number;
}

/**
 * Default audio configuration
 * These values are used when the app starts
 */
export const DEFAULT_AUDIO_CONFIG: AudioSettings = {
  sampleRate: 8000,     // Hz - matches ESP32 output
  bitDepth: 16,         // bits
  channels: 1,          // 1 = mono (ESP32 sends mono audio)
  bufferSize: 2048,     // samples
  gain: 1.0,            // volume multiplier
};

/**
 * Audio configuration constraints and validation
 */
export const AUDIO_CONSTRAINTS = {
  // Supported sample rates (Hz)
  SUPPORTED_SAMPLE_RATES: [8000, 16000, 22050, 44100, 48000] as const,

  // Supported bit depths
  SUPPORTED_BIT_DEPTHS: [8, 16, 24] as const,

  // Channel configurations
  SUPPORTED_CHANNELS: [1, 2] as const,

  // Buffer size constraints
  MIN_BUFFER_SIZE: 512,
  MAX_BUFFER_SIZE: 8192,

  // Gain constraints
  MIN_GAIN: 0.0,
  MAX_GAIN: 2.0,
};

/**
 * BLE-specific audio constants
 */
export const BLE_AUDIO_CONFIG = {
  // Maximum audio data size per BLE packet (bytes)
  MAX_CHUNK_SIZE: 240,

  // Timeout for incomplete audio frames (ms)
  FRAME_TIMEOUT_MS: 2000,

  // Maximum number of incomplete frames to keep in memory
  MAX_INCOMPLETE_FRAMES: 10,
};

/**
 * Validates audio settings against constraints
 */
export function validateAudioSettings(settings: Partial<AudioSettings>): boolean {
  if (settings.sampleRate !== undefined) {
    if (!AUDIO_CONSTRAINTS.SUPPORTED_SAMPLE_RATES.includes(settings.sampleRate as any)) {
      console.warn(`Unsupported sample rate: ${settings.sampleRate}`);
      return false;
    }
  }

  if (settings.bitDepth !== undefined) {
    if (!AUDIO_CONSTRAINTS.SUPPORTED_BIT_DEPTHS.includes(settings.bitDepth as any)) {
      console.warn(`Unsupported bit depth: ${settings.bitDepth}`);
      return false;
    }
  }

  if (settings.channels !== undefined) {
    if (!AUDIO_CONSTRAINTS.SUPPORTED_CHANNELS.includes(settings.channels as any)) {
      console.warn(`Unsupported channel configuration: ${settings.channels}`);
      return false;
    }
  }

  if (settings.bufferSize !== undefined) {
    if (settings.bufferSize < AUDIO_CONSTRAINTS.MIN_BUFFER_SIZE ||
        settings.bufferSize > AUDIO_CONSTRAINTS.MAX_BUFFER_SIZE) {
      console.warn(`Buffer size out of range: ${settings.bufferSize}`);
      return false;
    }
  }

  if (settings.gain !== undefined) {
    if (settings.gain < AUDIO_CONSTRAINTS.MIN_GAIN ||
        settings.gain > AUDIO_CONSTRAINTS.MAX_GAIN) {
      console.warn(`Gain out of range: ${settings.gain}`);
      return false;
    }
  }

  return true;
}

/**
 * Creates a complete AudioSettings object by merging partial settings
 * with defaults
 */
export function createAudioSettings(partial?: Partial<AudioSettings>): AudioSettings {
  return {
    ...DEFAULT_AUDIO_CONFIG,
    ...partial,
  };
}