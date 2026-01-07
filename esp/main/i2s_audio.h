/**
 * @file i2s_audio.h
 * @brief I2S audio interface for INMP441 microphone on ESP32-C3
 *
 * This module provides a simple interface for reading audio data from an
 * INMP441 I2S microphone. It handles the conversion from 32-bit I2S data
 * to 16-bit PCM samples.
 */

#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the I2S audio interface
 *
 * Configures the I2S peripheral for the INMP441 microphone with the following
 * settings:
 * - GPIO pins: SCK=6, WS=5, SD=4
 * - Bit width: 32-bit I2S input (converted to 16-bit output)
 * - Channel: Mono (left channel)
 * - DMA: 4 descriptors, 256 samples per frame
 * - Format: I2S Philips standard
 *
 * @param sample_rate Sample rate in Hz (typically 8000, 16000, 32000, 44100)
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_ARG if sample_rate is invalid
 *   - ESP_ERR_NO_MEM if allocation fails
 *   - Other ESP_ERR_* codes from I2S driver
 */
esp_err_t i2s_audio_init(uint32_t sample_rate);

/**
 * @brief Read audio samples from the I2S interface
 *
 * Reads audio data from the microphone and converts from 32-bit I2S format
 * to 16-bit PCM samples. This function blocks until data is available or
 * the timeout expires.
 *
 * @param buffer Buffer to store 16-bit PCM samples
 * @param num_samples Number of samples to read
 * @param samples_read Pointer to store actual number of samples read (can be NULL)
 * @param timeout_ms Timeout in milliseconds (0 = wait forever)
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_ARG if buffer is NULL or num_samples is 0
 *   - ESP_ERR_INVALID_STATE if I2S not initialized
 *   - ESP_ERR_TIMEOUT if no data available within timeout
 *   - Other ESP_ERR_* codes from I2S driver
 *
 * @note samples_read is always set to the actual number of samples read,
 *       even if an error occurs
 */
esp_err_t i2s_audio_read(int16_t *buffer, size_t num_samples,
                         size_t *samples_read, uint32_t timeout_ms);

/**
 * @brief Deinitialize the I2S audio interface
 *
 * Disables the I2S channel and frees all resources. After calling this
 * function, i2s_audio_init() must be called again before reading audio data.
 */
void i2s_audio_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // I2S_AUDIO_H
