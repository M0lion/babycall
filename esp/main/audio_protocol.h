/**
 * @file audio_protocol.h
 * @brief Audio streaming packet protocol definitions
 *
 * This header defines the binary packet format used for streaming audio
 * data over BLE. The protocol supports packet fragmentation to handle
 * MTU limitations.
 */

#ifndef AUDIO_PROTOCOL_H
#define AUDIO_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio packet protocol version
 */
#define AUDIO_PROTOCOL_VERSION 0x01

/**
 * @brief Audio packet header structure
 *
 * Binary format: [version(1B)|sequence(2B)|fragment(1B)|payload_len(2B)]
 * Total header size: 6 bytes
 *
 * Fragment byte format:
 * - Bit 7: More fragments flag (1=more fragments coming, 0=last/only fragment)
 * - Bits 6-4: Reserved (must be 0)
 * - Bits 3-0: Fragment index (0-15)
 */
typedef struct {
    uint8_t version;        // Protocol version (AUDIO_PROTOCOL_VERSION)
    uint16_t sequence;      // Sequence number (wraps at 65535)
    uint8_t fragment;       // Fragment info (bit 7: more flag, bits 3-0: index)
    uint16_t payload_len;   // Length of audio data following this header
} __attribute__((packed)) audio_packet_header_t;

/**
 * @brief Size of the audio packet header
 */
#define AUDIO_PACKET_HEADER_SIZE (sizeof(audio_packet_header_t))

/**
 * @brief Fragment flag bit masks
 */
#define AUDIO_FRAG_MORE_FLAG    0x80  // Bit 7: more fragments coming
#define AUDIO_FRAG_INDEX_MASK   0x0F  // Bits 3-0: fragment index

/**
 * @brief Helper macro to create fragment byte
 * @param more_fragments true if more fragments follow, false if last
 * @param index Fragment index (0-15)
 */
#define AUDIO_MAKE_FRAGMENT(more_fragments, index) \
    (((more_fragments) ? AUDIO_FRAG_MORE_FLAG : 0) | ((index) & AUDIO_FRAG_INDEX_MASK))

/**
 * @brief Helper macro to check if more fragments follow
 * @param frag Fragment byte
 */
#define AUDIO_HAS_MORE_FRAGMENTS(frag) \
    (((frag) & AUDIO_FRAG_MORE_FLAG) != 0)

/**
 * @brief Helper macro to extract fragment index
 * @param frag Fragment byte
 */
#define AUDIO_GET_FRAGMENT_INDEX(frag) \
    ((frag) & AUDIO_FRAG_INDEX_MASK)

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PROTOCOL_H
