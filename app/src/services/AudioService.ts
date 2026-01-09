/**
 * AudioService - PCM Audio Playback Service
 *
 * Provides continuous audio playback with scheduling using react-native-audio-api.
 * Based on the Web Audio API pattern from the WiFi example web client.
 */

import {
	AudioContext,
	GainNode,
	AudioBufferSourceNode,
} from 'react-native-audio-api';
import { DEFAULT_AUDIO_CONFIG, AudioSettings } from '../config';

export interface AudioStats {
	framesPlayed: number;
	underruns: number;
	bufferHealth: number;
	sampleRate: number;
}

export class AudioService {
	private context: AudioContext | null = null;
	private gainNode: GainNode | null = null;
	private currentConfig: AudioSettings = DEFAULT_AUDIO_CONFIG;
	private nextPlayTime: number = 0;
	private isInitialized: boolean = false;
	private playing: boolean = false;

	// Statistics
	private stats: AudioStats = {
		framesPlayed: 0,
		underruns: 0,
		bufferHealth: 0,
		sampleRate: DEFAULT_AUDIO_CONFIG.sampleRate,
	};

	/**
	 * Initialize the audio context with configuration
	 * @param config Partial audio settings to override defaults
	 */
	init(config?: Partial<AudioSettings>): void {
		try {
			if (this.isInitialized) {
				console.warn('[AudioService] Already initialized, reinitializing...');
				this.dispose();
			}

			// Merge with defaults
			this.currentConfig = {
				...DEFAULT_AUDIO_CONFIG,
				...config,
			};

			this.stats.sampleRate = this.currentConfig.sampleRate;

			// Create audio context with specified sample rate
			this.context = new AudioContext({ sampleRate: this.currentConfig.sampleRate });

			// Create gain node for volume control
			this.gainNode = this.context.createGain();
			this.gainNode.gain.value = this.currentConfig.gain || 1.0;
			this.gainNode.connect(this.context.destination);

			// Resume context if suspended
			if (this.context.state === 'suspended') {
				this.context.resume().catch((err) => {
					console.error('[AudioService] Failed to resume context:', err);
				});
			}

			this.isInitialized = true;
			this.playing = false;
			this.nextPlayTime = 0;

			console.log('[AudioService] Initialized with config:', this.currentConfig);
		} catch (error) {
			console.error('[AudioService] Initialization failed:', error);
			throw new Error(`Failed to initialize AudioService: ${error}`);
		}
	}

	/**
	 * Reinitialize with new configuration (for runtime changes)
	 * @param config New audio settings
	 */
	reinitialize(config: Partial<AudioSettings>): void {
		console.log('[AudioService] Reinitializing with new config:', config);
		this.init(config);
	}

	/**
	 * Play a PCM audio frame
	 * @param samples Int16Array of audio samples
	 */
	playPCMFrame(samples: Int16Array): void {
		if (!this.isInitialized || !this.context || !this.gainNode) {
			console.error('[AudioService] Not initialized');
			return;
		}

		try {
			// Resume context if suspended
			if (this.context.state === 'suspended') {
				this.context.resume().catch((err) => {
					console.error('[AudioService] Failed to resume context:', err);
				});
			}

			// Convert Int16 to Float32 (divide by 32768)
			const float32 = new Float32Array(samples.length);
			for (let i = 0; i < samples.length; i++) {
				float32[i] = samples[i] / 32768.0;
			}

			// Create audio buffer
			const buffer = this.context.createBuffer(
				1, // Mono
				float32.length,
				this.currentConfig.sampleRate
			);

			// Copy data to buffer
			buffer.getChannelData(0).set(float32);

			// Create buffer source
			const source = this.context.createBufferSource();
			source.buffer = buffer;
			source.connect(this.gainNode);

			// Schedule playback (Web Audio API pattern from WiFi example)
			const now = this.context.currentTime;

			// Detect underrun (buffer gap)
			if (this.playing && this.nextPlayTime < now) {
				this.stats.underruns++;
				this.nextPlayTime = now;
				console.warn(
					`[AudioService] Buffer underrun detected. Total: ${this.stats.underruns}`
				);
			}

			// Initialize nextPlayTime on first frame
			if (!this.playing || this.nextPlayTime < now) {
				this.nextPlayTime = now;
			}

			// Schedule and start playback
			source.start(this.nextPlayTime);
			this.nextPlayTime += buffer.duration;
			this.playing = true;

			// Update statistics
			this.stats.framesPlayed++;
			this.stats.bufferHealth = Math.max(0, this.nextPlayTime - now);
		} catch (error) {
			console.error('[AudioService] Playback error:', error);
			// Don't throw, just log - allow continued operation
		}
	}

	/**
	 * Get buffer health (seconds of buffered audio)
	 * @returns Seconds of audio buffered ahead
	 */
	getBufferHealth(): number {
		if (!this.context || !this.playing) {
			return 0;
		}

		const health = Math.max(0, this.nextPlayTime - this.context.currentTime);
		this.stats.bufferHealth = health;
		return health;
	}

	/**
	 * Stop audio playback and reset
	 */
	stop(): void {
		if (!this.isInitialized) {
			return;
		}

		try {
			this.playing = false;
			this.nextPlayTime = 0;

			// Context remains initialized but playback is stopped
			console.log('[AudioService] Playback stopped');
		} catch (error) {
			console.error('[AudioService] Stop error:', error);
		}
	}

	/**
	 * Set playback volume
	 * @param volume Volume level 0.0 to 1.0
	 */
	setVolume(volume: number): void {
		if (!this.gainNode) {
			console.error('[AudioService] Not initialized');
			return;
		}

		try {
			const clampedVolume = Math.max(0, Math.min(1, volume));
			this.gainNode.gain.value = clampedVolume;
			console.log('[AudioService] Volume set to:', clampedVolume);
		} catch (error) {
			console.error('[AudioService] Set volume error:', error);
		}
	}

	/**
	 * Check if audio is currently playing
	 * @returns True if playing
	 */
	isPlaying(): boolean {
		return this.playing;
	}

	/**
	 * Get playback statistics
	 * @returns AudioStats object
	 */
	getStats(): AudioStats {
		return {
			...this.stats,
			bufferHealth: this.getBufferHealth(),
		};
	}

	/**
	 * Get current audio configuration
	 * @returns Current AudioSettings
	 */
	getConfig(): AudioSettings {
		return { ...this.currentConfig };
	}

	/**
	 * Clean up resources (call when done with service)
	 */
	dispose(): void {
		try {
			this.stop();

			if (this.gainNode) {
				this.gainNode.disconnect();
				this.gainNode = null;
			}

			if (this.context) {
				this.context.close().catch((err) => {
					console.error('[AudioService] Context close error:', err);
				});
				this.context = null;
			}

			this.isInitialized = false;
			console.log('[AudioService] Disposed');
		} catch (error) {
			console.error('[AudioService] Dispose error:', error);
		}
	}

	/**
	 * Reset statistics
	 */
	resetStats(): void {
		this.stats.framesPlayed = 0;
		this.stats.underruns = 0;
		this.stats.bufferHealth = 0;
		console.log('[AudioService] Statistics reset');
	}
}

// Export singleton instance
export default new AudioService();
