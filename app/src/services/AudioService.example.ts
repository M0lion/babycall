/**
 * AudioService Usage Example
 *
 * This file demonstrates how to use the AudioService for PCM audio playback.
 */

import audioService from './AudioService';

/**
 * Example 1: Basic playback setup
 */
export function exampleBasicSetup() {
	// Initialize with 16kHz sample rate
	audioService.init(16000);

	// Set volume to 80%
	audioService.setVolume(0.8);

	console.log('AudioService initialized and ready');
}

/**
 * Example 2: Play audio from Int16Array
 */
export function examplePlayAudio(pcmData: Int16Array) {
	// Simply pass the Int16Array to the service
	// The service handles conversion and scheduling
	audioService.playPCMFrame(pcmData);
}

/**
 * Example 3: Monitor buffer health
 */
export function exampleMonitorBuffer() {
	const bufferSeconds = audioService.getBufferHealth();

	if (bufferSeconds < 0.1) {
		console.warn('Buffer is running low!', bufferSeconds);
	} else {
		console.log(`Buffer healthy: ${bufferSeconds.toFixed(3)}s`);
	}
}

/**
 * Example 4: Get statistics
 */
export function exampleGetStats() {
	const stats = audioService.getStats();

	console.log('=== Audio Statistics ===');
	console.log(`Frames played: ${stats.framesPlayed}`);
	console.log(`Underruns: ${stats.underruns}`);
	console.log(`Buffer health: ${stats.bufferHealth.toFixed(3)}s`);
	console.log(`Sample rate: ${stats.sampleRate} Hz`);
}

/**
 * Example 5: BLE Audio Stream Handler
 */
export class BLEAudioStreamHandler {
	private isStreaming: boolean = false;

	start() {
		// Initialize audio service
		audioService.init(8000);
		audioService.setVolume(1.0);
		this.isStreaming = true;

		console.log('BLE audio stream started');
	}

	handleAudioPacket(data: ArrayBuffer) {
		if (!this.isStreaming) return;

		// Convert ArrayBuffer to Int16Array
		const samples = new Int16Array(data);

		// Play the audio frame
		audioService.playPCMFrame(samples);

		// Monitor buffer health
		const health = audioService.getBufferHealth();
		if (health < 0.05) {
			console.warn('Buffer critical!');
		}
	}

	stop() {
		this.isStreaming = false;
		audioService.stop();

		// Show final statistics
		const stats = audioService.getStats();
		console.log('Stream ended:', stats);
	}

	dispose() {
		this.stop();
		audioService.dispose();
	}
}

/**
 * Example 6: Generate test tone (for testing)
 */
export function exampleGenerateTestTone(frequency: number = 440, durationMs: number = 100) {
	const sampleRate = 16000;
	const samples = Math.floor((sampleRate * durationMs) / 1000);
	const pcmData = new Int16Array(samples);

	// Generate a sine wave
	for (let i = 0; i < samples; i++) {
		const t = i / sampleRate;
		const value = Math.sin(2 * Math.PI * frequency * t);
		pcmData[i] = Math.floor(value * 32767);
	}

	return pcmData;
}

/**
 * Example 7: Full playback test
 */
export function exampleFullTest() {
	console.log('Starting AudioService test...');

	// Initialize
	audioService.init(16000);
	audioService.setVolume(0.5);

	// Generate and play test tones
	const tone1 = exampleGenerateTestTone(440, 200); // A4
	const tone2 = exampleGenerateTestTone(523, 200); // C5
	const tone3 = exampleGenerateTestTone(659, 200); // E5

	// Play sequence
	audioService.playPCMFrame(tone1);
	audioService.playPCMFrame(tone2);
	audioService.playPCMFrame(tone3);

	// Check stats
	console.log('Playing test tones...');
	setTimeout(() => {
		const stats = audioService.getStats();
		console.log('Test complete:', stats);
	}, 1000);
}

/**
 * Example 8: Error handling
 */
export function exampleErrorHandling() {
	try {
		// Initialize
		audioService.init(16000);

		// Simulate error conditions
		const emptyBuffer = new Int16Array(0);
		audioService.playPCMFrame(emptyBuffer); // Should handle gracefully

		// Check if playing
		if (audioService.isPlaying()) {
			console.log('Audio is playing');
		}

		// Clean stop
		audioService.stop();
	} catch (error) {
		console.error('Error during playback:', error);
	}
}
