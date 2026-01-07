/**
 * @file AudioProtocol.ts
 * @brief Audio streaming packet protocol implementation for React Native
 *
 * This module handles parsing and reassembly of audio packets received from
 * the ESP32 baby monitor device over BLE. It matches the protocol defined in
 * the ESP32 firmware (audio_protocol.h).
 *
 * Packet Format: [version(1B) | sequence(2B) | fragment(1B) | payload_len(2B) | audio_data(variable)]
 * Fragment byte: Bit 7 = more fragments, Bits 3-0 = fragment index (0-15)
 */

// Protocol constants
const AUDIO_PROTOCOL_VERSION = 0x01;
const AUDIO_PACKET_HEADER_SIZE = 6;
const AUDIO_FRAG_MORE_FLAG = 0x80;
const AUDIO_FRAG_INDEX_MASK = 0x0f;
const MAX_FRAGMENT_INDEX = 15;
const FRAME_TIMEOUT_MS = 2000;
const MAX_INCOMPLETE_FRAMES = 10;

/**
 * Parsed audio packet structure
 */
export interface AudioPacket {
  version: number;
  sequence: number;
  fragmentIndex: number;
  isLast: boolean;
  data: Int16Array;
}

/**
 * Internal structure for tracking incomplete frames
 */
interface IncompleteFrame {
  sequence: number;
  fragments: Map<number, Int16Array>;
  totalFragments: number;
  timestamp: number;
}

/**
 * Statistics for monitoring protocol performance
 */
export interface AudioProtocolStats {
  totalPackets: number;
  totalFrames: number;
  droppedFrames: number;
  packetLossRate: number;
  lastSequence: number;
  expectedPackets: number;
  receivedPackets: number;
}

/**
 * AudioProtocol class handles packet parsing and frame reassembly
 */
export class AudioProtocol {
  private incompleteFrames: Map<number, IncompleteFrame>;
  private lastSequence: number | null;
  private stats: AudioProtocolStats;
  private lastCleanupTime: number;

  constructor() {
    this.incompleteFrames = new Map();
    this.lastSequence = null;
    this.lastCleanupTime = Date.now();
    this.stats = {
      totalPackets: 0,
      totalFrames: 0,
      droppedFrames: 0,
      packetLossRate: 0,
      lastSequence: 0,
      expectedPackets: 0,
      receivedPackets: 0,
    };
  }

  /**
   * Parse a raw packet from BLE into an AudioPacket structure
   * @param rawData Raw packet bytes from BLE
   * @returns Parsed packet or null if invalid
   */
  parsePacket(rawData: Uint8Array): AudioPacket | null {
    // Validate minimum packet size
    if (rawData.length < AUDIO_PACKET_HEADER_SIZE) {
      console.warn(
        `[AudioProtocol] Invalid packet size: ${rawData.length} bytes (min ${AUDIO_PACKET_HEADER_SIZE})`
      );
      return null;
    }

    // Parse header using DataView (little-endian)
    const view = new DataView(
      rawData.buffer,
      rawData.byteOffset,
      rawData.byteLength
    );

    const version = view.getUint8(0);
    const sequence = view.getUint16(1, true); // little-endian
    const fragment = view.getUint8(3);
    const payloadLen = view.getUint16(4, true); // little-endian

    // Validate version
    if (version !== AUDIO_PROTOCOL_VERSION) {
      console.warn(
        `[AudioProtocol] Version mismatch: got ${version}, expected ${AUDIO_PROTOCOL_VERSION}`
      );
      return null;
    }

    // Extract fragment info
    const fragmentIndex = fragment & AUDIO_FRAG_INDEX_MASK;
    const hasMoreFragments = (fragment & AUDIO_FRAG_MORE_FLAG) !== 0;
    const isLast = !hasMoreFragments;

    // Validate fragment index
    if (fragmentIndex > MAX_FRAGMENT_INDEX) {
      console.error(
        `[AudioProtocol] Fragment index out of range: ${fragmentIndex} (max ${MAX_FRAGMENT_INDEX}), discarding frame ${sequence}`
      );
      // Remove incomplete frame if it exists
      this.incompleteFrames.delete(sequence);
      return null;
    }

    // Validate payload length
    const expectedSize = AUDIO_PACKET_HEADER_SIZE + payloadLen;
    if (rawData.length < expectedSize) {
      console.warn(
        `[AudioProtocol] Payload length mismatch: expected ${expectedSize}, got ${rawData.length}`
      );
      return null;
    }

    // Extract audio data as Int16Array
    const audioDataOffset = AUDIO_PACKET_HEADER_SIZE;
    const sampleCount = payloadLen / 2; // 2 bytes per Int16 sample
    const audioData = new Int16Array(
      rawData.buffer,
      rawData.byteOffset + audioDataOffset,
      sampleCount
    );

    // Update statistics
    this.stats.totalPackets++;
    this.stats.receivedPackets++;

    return {
      version,
      sequence,
      fragmentIndex,
      isLast,
      data: audioData,
    };
  }

  /**
   * Reassemble a complete audio frame from packet fragments
   * @param packet Parsed audio packet
   * @returns Complete audio frame or null if not yet complete
   */
  reassembleFrame(packet: AudioPacket): Int16Array | null {
    const { sequence, fragmentIndex, isLast, data } = packet;

    // Update packet loss statistics
    this.updatePacketLossStats(sequence);

    // Handle single-fragment frames (no fragmentation)
    if (fragmentIndex === 0 && isLast) {
      this.stats.totalFrames++;
      return data;
    }

    // Get or create incomplete frame entry
    let frame = this.incompleteFrames.get(sequence);
    if (!frame) {
      frame = {
        sequence,
        fragments: new Map(),
        totalFragments: -1, // Unknown until we receive the last fragment
        timestamp: Date.now(),
      };
      this.incompleteFrames.set(sequence, frame);

      // Enforce max incomplete frames limit
      this.enforceFrameLimit();
    }

    // Store fragment
    frame.fragments.set(fragmentIndex, data);
    frame.timestamp = Date.now(); // Update timestamp on each fragment

    // If this is the last fragment, we now know the total count
    if (isLast) {
      frame.totalFragments = fragmentIndex + 1;
    }

    // Check if frame is complete
    if (frame.totalFragments > 0 && frame.fragments.size === frame.totalFragments) {
      // Reassemble fragments in order
      let totalSamples = 0;
      for (let i = 0; i < frame.totalFragments; i++) {
        const fragment = frame.fragments.get(i);
        if (!fragment) {
          // Missing fragment - this shouldn't happen if we checked size correctly
          console.error(
            `[AudioProtocol] Missing fragment ${i} for sequence ${sequence}`
          );
          this.incompleteFrames.delete(sequence);
          this.stats.droppedFrames++;
          return null;
        }
        totalSamples += fragment.length;
      }

      // Create combined buffer
      const completeFrame = new Int16Array(totalSamples);
      let offset = 0;
      for (let i = 0; i < frame.totalFragments; i++) {
        const fragment = frame.fragments.get(i)!;
        completeFrame.set(fragment, offset);
        offset += fragment.length;
      }

      // Clean up
      this.incompleteFrames.delete(sequence);
      this.stats.totalFrames++;

      return completeFrame;
    }

    // Frame not yet complete
    return null;
  }

  /**
   * Process incoming raw data: parse packet and reassemble frame
   * @param rawData Raw packet bytes from BLE
   * @returns Complete audio frame or null if not yet complete/invalid
   */
  processIncomingData(rawData: Uint8Array): Int16Array | null {
    // Clean up old frames periodically
    this.cleanupOldFrames();

    // Parse packet
    const packet = this.parsePacket(rawData);
    if (!packet) {
      return null;
    }

    // Reassemble frame
    return this.reassembleFrame(packet);
  }

  /**
   * Reset the protocol state
   */
  reset(): void {
    this.incompleteFrames.clear();
    this.lastSequence = null;
    this.lastCleanupTime = Date.now();
    this.stats = {
      totalPackets: 0,
      totalFrames: 0,
      droppedFrames: 0,
      packetLossRate: 0,
      lastSequence: 0,
      expectedPackets: 0,
      receivedPackets: 0,
    };
    console.log('[AudioProtocol] Reset complete');
  }

  /**
   * Get current packet loss rate (0.0 to 1.0)
   * @returns Packet loss rate as a decimal (e.g., 0.05 = 5%)
   */
  getPacketLossRate(): number {
    return this.stats.packetLossRate;
  }

  /**
   * Get detailed statistics
   * @returns Current protocol statistics
   */
  getStats(): AudioProtocolStats {
    return { ...this.stats };
  }

  /**
   * Update packet loss statistics based on sequence number
   * @param sequence Current sequence number
   */
  private updatePacketLossStats(sequence: number): void {
    if (this.lastSequence !== null) {
      // Calculate expected next sequence (with wrapping at 65536)
      const expectedSequence = (this.lastSequence + 1) % 65536;

      // Calculate missed packets (accounting for wrapping)
      let missed = 0;
      if (sequence !== expectedSequence) {
        if (sequence > expectedSequence) {
          missed = sequence - expectedSequence;
        } else {
          // Wrapped around
          missed = 65536 - expectedSequence + sequence;
        }

        // Sanity check: if gap is huge, likely reset/restart
        if (missed > 1000) {
          console.warn(
            `[AudioProtocol] Large sequence gap detected (${missed}), possible reset`
          );
          missed = 0;
        }
      }

      this.stats.expectedPackets += missed + 1;
    } else {
      // First packet
      this.stats.expectedPackets = 1;
    }

    this.lastSequence = sequence;
    this.stats.lastSequence = sequence;

    // Calculate packet loss rate
    if (this.stats.expectedPackets > 0) {
      const lostPackets = this.stats.expectedPackets - this.stats.receivedPackets;
      this.stats.packetLossRate = lostPackets / this.stats.expectedPackets;
    }
  }

  /**
   * Clean up old incomplete frames that have timed out
   */
  private cleanupOldFrames(): void {
    const now = Date.now();

    // Only cleanup every second to avoid excessive processing
    if (now - this.lastCleanupTime < 1000) {
      return;
    }

    this.lastCleanupTime = now;

    // Find and remove timed-out frames
    const timeoutThreshold = now - FRAME_TIMEOUT_MS;
    const toDelete: number[] = [];

    this.incompleteFrames.forEach((frame, sequence) => {
      if (frame.timestamp < timeoutThreshold) {
        toDelete.push(sequence);
        this.stats.droppedFrames++;
      }
    });

    toDelete.forEach((sequence) => {
      console.warn(
        `[AudioProtocol] Dropping incomplete frame ${sequence} (timeout)`
      );
      this.incompleteFrames.delete(sequence);
    });
  }

  /**
   * Enforce maximum number of incomplete frames in memory
   */
  private enforceFrameLimit(): void {
    if (this.incompleteFrames.size <= MAX_INCOMPLETE_FRAMES) {
      return;
    }

    // Find oldest frame by timestamp
    let oldestSequence: number | null = null;
    let oldestTimestamp = Infinity;

    this.incompleteFrames.forEach((frame, sequence) => {
      if (frame.timestamp < oldestTimestamp) {
        oldestTimestamp = frame.timestamp;
        oldestSequence = sequence;
      }
    });

    // Remove oldest frame
    if (oldestSequence !== null) {
      console.warn(
        `[AudioProtocol] Max incomplete frames reached, dropping oldest frame ${oldestSequence}`
      );
      this.incompleteFrames.delete(oldestSequence);
      this.stats.droppedFrames++;
    }
  }
}

// Export singleton instance for convenience
export default new AudioProtocol();
