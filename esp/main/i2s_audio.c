/**
 * @file i2s_audio.c
 * @brief I2S audio interface implementation for INMP441 microphone
 */

#include "i2s_audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "I2S_AUDIO";

// GPIO pin definitions for INMP441 microphone
#define I2S_SCK_PIN 6  // Serial Clock (BCLK)
#define I2S_WS_PIN 5   // Word Select (LRCK)
#define I2S_SD_PIN 4   // Serial Data

// DMA configuration
#define I2S_DMA_DESC_NUM 4    // Number of DMA descriptors
#define I2S_DMA_FRAME_NUM 256 // Number of samples per DMA frame

// I2S channel handle
static i2s_chan_handle_t rx_chan = NULL;

esp_err_t i2s_audio_init(uint32_t sample_rate) {
  esp_err_t ret;

  // Validate sample rate
  if (sample_rate == 0) {
    ESP_LOGE(TAG, "Invalid sample rate: %lu", sample_rate);
    return ESP_ERR_INVALID_ARG;
  }

  // Check if already initialized
  if (rx_chan != NULL) {
    ESP_LOGW(TAG, "I2S already initialized, deinitializing first");
    i2s_audio_deinit();
  }

  ESP_LOGI(TAG, "Initializing I2S audio interface at %lu Hz", sample_rate);

  // Configure I2S channel
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = I2S_DMA_DESC_NUM;
  chan_cfg.dma_frame_num = I2S_DMA_FRAME_NUM;

  ret = i2s_new_channel(&chan_cfg, NULL, &rx_chan);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
    return ret;
  }

  // Configure I2S standard mode (Philips format)
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                      I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = I2S_SCK_PIN,
              .ws = I2S_WS_PIN,
              .dout = I2S_GPIO_UNUSED,
              .din = I2S_SD_PIN,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  // Select left channel for mono recording
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  ret = i2s_channel_init_std_mode(rx_chan, &std_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s",
             esp_err_to_name(ret));
    i2s_del_channel(rx_chan);
    rx_chan = NULL;
    return ret;
  }

  // Enable the I2S channel
  ret = i2s_channel_enable(rx_chan);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
    i2s_del_channel(rx_chan);
    rx_chan = NULL;
    return ret;
  }

  ESP_LOGI(TAG, "I2S audio interface initialized successfully");
  ESP_LOGI(TAG, "GPIO pins - SCK:%d WS:%d SD:%d", I2S_SCK_PIN, I2S_WS_PIN,
           I2S_SD_PIN);

  return ESP_OK;
}

esp_err_t i2s_audio_read(int16_t *buffer, size_t num_samples,
                         size_t *samples_read, uint32_t timeout_ms) {
  esp_err_t ret;
  size_t bytes_read = 0;
  int32_t *temp_buffer = NULL;

  // Initialize samples_read to 0
  if (samples_read != NULL) {
    *samples_read = 0;
  }

  // Validate parameters
  if (buffer == NULL) {
    ESP_LOGE(TAG, "Buffer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (num_samples == 0) {
    ESP_LOGE(TAG, "Number of samples is 0");
    return ESP_ERR_INVALID_ARG;
  }

  // Check if initialized
  if (rx_chan == NULL) {
    ESP_LOGE(TAG, "I2S not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  // Allocate temporary buffer for 32-bit samples
  size_t temp_buffer_size = num_samples * sizeof(int32_t);
  temp_buffer = (int32_t *)malloc(temp_buffer_size);
  if (temp_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate temporary buffer (%zu bytes)",
             temp_buffer_size);
    return ESP_ERR_NO_MEM;
  }

  // Calculate timeout in ticks
  TickType_t timeout_ticks;
  if (timeout_ms == 0) {
    timeout_ticks = portMAX_DELAY; // Wait forever
  } else {
    timeout_ticks = pdMS_TO_TICKS(timeout_ms);
  }

  // Read 32-bit samples from I2S
  ret = i2s_channel_read(rx_chan, temp_buffer, temp_buffer_size, &bytes_read,
                         timeout_ticks);

  if (ret != ESP_OK) {
    if (ret == ESP_ERR_TIMEOUT) {
      ESP_LOGD(TAG, "I2S read timeout after %lu ms", timeout_ms);
    } else {
      ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(ret));
    }
    free(temp_buffer);
    return ret;
  }

  // Convert 32-bit samples to 16-bit
  size_t samples_converted = bytes_read / sizeof(int32_t);
  for (size_t i = 0; i < samples_converted; i++) {
    // Right-shift by 16 bits to convert from 32-bit to 16-bit
    buffer[i] = (int16_t)(temp_buffer[i] >> 16);
  }

  // Set the number of samples read
  if (samples_read != NULL) {
    *samples_read = samples_converted;
  }

  free(temp_buffer);

  // Log warning if we read fewer samples than requested
  if (samples_converted < num_samples) {
    ESP_LOGD(TAG, "Read %zu samples (requested %zu)", samples_converted,
             num_samples);
  }

  return ESP_OK;
}

void i2s_audio_deinit(void) {
  if (rx_chan == NULL) {
    ESP_LOGW(TAG, "I2S not initialized, nothing to deinitialize");
    return;
  }

  ESP_LOGI(TAG, "Deinitializing I2S audio interface");

  // Disable the I2S channel
  esp_err_t ret = i2s_channel_disable(rx_chan);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to disable I2S channel: %s", esp_err_to_name(ret));
  }

  // Delete the I2S channel
  ret = i2s_del_channel(rx_chan);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to delete I2S channel: %s", esp_err_to_name(ret));
  }

  rx_chan = NULL;
  ESP_LOGI(TAG, "I2S audio interface deinitialized");
}
