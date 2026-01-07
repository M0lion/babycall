/**
 * @file audio_streamer.h
 * @brief Audio streaming pipeline for BLE transmission
 *
 * This module implements the audio streaming pipeline that connects I2S
 * audio capture with BLE transmission. It handles packet fragmentation,
 * sequence numbering, and automatic streaming based on BLE connection state.
 */

#ifndef AUDIO_STREAMER_H
#define AUDIO_STREAMER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio streaming statistics
 *
 * This structure contains real-time statistics about the audio streaming
 * operation. Statistics are reset when streaming starts.
 */
typedef struct {
    uint64_t frames_sent;           // Total number of audio frames sent
    uint64_t bytes_sent;            // Total number of bytes sent (including headers)
    uint64_t packets_sent;          // Total number of BLE packets sent
    uint64_t fragments_sent;        // Total number of fragments sent
    uint32_t errors;                // Number of errors encountered
    uint32_t frames_dropped;        // Number of frames dropped (BLE not ready)
    uint32_t uptime_seconds;        // Seconds since streaming started
    bool is_streaming;              // Current streaming state
} audio_streamer_stats_t;

/**
 * @brief Initialize the audio streamer
 *
 * Initializes the audio streamer module. This must be called before
 * starting the streaming task. This function initializes the I2S audio
 * interface with a sample rate of 16000 Hz.
 *
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_STATE if already initialized
 *   - ESP_ERR_NO_MEM if allocation fails
 *   - Other ESP_ERR_* codes from I2S initialization
 */
esp_err_t audio_streamer_init(void);

/**
 * @brief Start the audio streaming task
 *
 * Creates and starts the FreeRTOS task that handles audio streaming.
 * The task will automatically stream audio when BLE is connected and
 * pause when disconnected.
 *
 * Task configuration:
 * - Priority: 5 (medium-high)
 * - Stack size: 4096 bytes
 *
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_STATE if not initialized or already started
 *   - ESP_ERR_NO_MEM if task creation fails
 */
esp_err_t audio_streamer_start(void);

/**
 * @brief Stop the audio streaming task
 *
 * Stops the streaming task gracefully. This will wait for the task
 * to finish its current operation and then delete it.
 */
void audio_streamer_stop(void);

/**
 * @brief Deinitialize the audio streamer
 *
 * Cleans up all resources used by the audio streamer. The streaming
 * task must be stopped before calling this function.
 */
void audio_streamer_deinit(void);

/**
 * @brief Get current streaming statistics
 *
 * Returns a snapshot of the current streaming statistics. This can be
 * called at any time, even when streaming is not active.
 *
 * @param stats Pointer to structure to receive statistics
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_ARG if stats is NULL
 *   - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t audio_streamer_get_stats(audio_streamer_stats_t *stats);

/**
 * @brief Reset streaming statistics
 *
 * Resets all streaming statistics counters to zero. The uptime counter
 * will restart from the time of this call.
 */
void audio_streamer_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_STREAMER_H
