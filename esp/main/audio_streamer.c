/**
 * @file audio_streamer.c
 * @brief Audio streaming pipeline implementation
 */

#include "audio_streamer.h"
#include "audio_protocol.h"
#include "ble/ble_simple.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_audio.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "AUDIO_STREAMER";

// Configuration constants
#define SAMPLE_RATE_HZ 8000
#define SAMPLES_PER_READ 128
#define BYTES_PER_READ (SAMPLES_PER_READ * sizeof(int16_t)) // 256 bytes
#define I2S_READ_TIMEOUT_MS 100
#define TASK_PRIORITY 5
#define TASK_STACK_SIZE 4096
#define STATS_LOG_INTERVAL_MS 1000

// Module state
typedef struct {
  bool initialized;
  bool running;
  TaskHandle_t task_handle;
  uint16_t sequence_number;
  audio_streamer_stats_t stats;
  int64_t stats_last_log_time;
  int64_t streaming_start_time;
  ble_char_handle_t audio_data_handle;
} audio_streamer_state_t;

static audio_streamer_state_t s_state = {0};

/**
 * @brief Get current time in milliseconds
 */
static int64_t get_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000LL + (int64_t)tv.tv_usec / 1000LL;
}

/**
 * @brief Log streaming statistics
 */
static void log_stats(void) {
  int64_t now = get_time_ms();

  // Update uptime
  if (s_state.streaming_start_time > 0) {
    s_state.stats.uptime_seconds =
        (uint32_t)((now - s_state.streaming_start_time) / 1000);
  }

  // Log every second
  if (now - s_state.stats_last_log_time >= STATS_LOG_INTERVAL_MS) {
    ESP_LOGI(TAG,
             "Stats: frames=%llu, packets=%llu, bytes=%llu, errors=%u, "
             "dropped=%u, uptime=%us",
             s_state.stats.frames_sent, s_state.stats.packets_sent,
             s_state.stats.bytes_sent, s_state.stats.errors,
             s_state.stats.frames_dropped, s_state.stats.uptime_seconds);
    s_state.stats_last_log_time = now;
  }
}

/**
 * @brief Fragment and send an audio frame via BLE
 *
 * @param audio_data Pointer to audio data buffer
 * @param audio_len Length of audio data in bytes
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t send_audio_frame(const uint8_t *audio_data, size_t audio_len) {
  // Get current MTU size
  uint16_t mtu = ble_simple_get_mtu();
  size_t max_packet_size = mtu - 3; // Account for ATT overhead
  size_t max_payload_size = max_packet_size - AUDIO_PACKET_HEADER_SIZE;

  // Calculate number of fragments needed
  size_t num_fragments = (audio_len + max_payload_size - 1) / max_payload_size;

  if (num_fragments > 16) {
    ESP_LOGW(TAG, "Frame too large: %d bytes, needs %d fragments (max 16)",
             audio_len, num_fragments);
    s_state.stats.errors++;
    return ESP_ERR_INVALID_SIZE;
  }

  // Allocate buffer for packet (header + max payload)
  uint8_t packet_buffer[max_packet_size];
  audio_packet_header_t *header = (audio_packet_header_t *)packet_buffer;
  uint8_t *payload = packet_buffer + AUDIO_PACKET_HEADER_SIZE;

  // Send fragments
  size_t bytes_sent = 0;
  for (size_t i = 0; i < num_fragments; i++) {
    // Calculate payload size for this fragment
    size_t remaining = audio_len - bytes_sent;
    size_t payload_size =
        (remaining < max_payload_size) ? remaining : max_payload_size;
    bool more_fragments = (i < num_fragments - 1);

    // Fill header
    header->version = AUDIO_PROTOCOL_VERSION;
    header->sequence = s_state.sequence_number;
    header->fragment = AUDIO_MAKE_FRAGMENT(more_fragments, i);
    header->payload_len = (uint16_t)payload_size;

    // Copy audio data to payload
    memcpy(payload, audio_data + bytes_sent, payload_size);

    // Calculate total packet size
    size_t packet_size = AUDIO_PACKET_HEADER_SIZE + payload_size;

    // Send packet via BLE
    if (ble_simple_is_connected()) {
      esp_err_t ret = ble_simple_notify(s_state.audio_data_handle,
                                        (uint8_t *)&packet_buffer, packet_size);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send audio: %s", esp_err_to_name(ret));
      }
    }

    // Update statistics
    s_state.stats.packets_sent++;
    s_state.stats.fragments_sent++;
    s_state.stats.bytes_sent += packet_size;
    bytes_sent += payload_size;
  }

  // Increment sequence number (wraps at 65535)
  s_state.sequence_number++;
  s_state.stats.frames_sent++;

  return ESP_OK;
}

/**
 * @brief Main audio streaming task
 */
static void audio_streaming_task(void *arg) {
  ESP_LOGI(TAG, "Audio streaming task started");

  // Allocate buffer for audio data
  int16_t audio_buffer[SAMPLES_PER_READ];
  size_t samples_read;

  // Reset statistics
  memset(&s_state.stats, 0, sizeof(s_state.stats));
  s_state.sequence_number = 0;
  s_state.stats_last_log_time = get_time_ms();
  s_state.streaming_start_time = 0;

  while (s_state.running) {
    // Check if BLE is connected
    if (!ble_simple_is_connected() ||
        !ble_simple_is_notify_enabled(s_state.audio_data_handle)) {
      // Not connected, update state and wait
      if (s_state.stats.is_streaming) {
        ESP_LOGI(TAG, "BLE disconnected, pausing streaming");
        s_state.stats.is_streaming = false;
        s_state.streaming_start_time = 0;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // BLE is connected, start/continue streaming
    if (!s_state.stats.is_streaming) {
      ESP_LOGI(TAG, "BLE connected, starting streaming");
      s_state.stats.is_streaming = true;
      s_state.streaming_start_time = get_time_ms();
      s_state.stats_last_log_time = s_state.streaming_start_time;
    }

    // Read audio from I2S
    esp_err_t ret = i2s_audio_read(audio_buffer, SAMPLES_PER_READ,
                                   &samples_read, I2S_READ_TIMEOUT_MS);

    if (ret != ESP_OK) {
      if (ret != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "I2S read error: %s", esp_err_to_name(ret));
        s_state.stats.errors++;
      }
      continue;
    }

    if (samples_read == 0) {
      continue;
    }

    // Send audio frame via BLE
    size_t bytes_read = samples_read * sizeof(int16_t);
    ret = send_audio_frame((uint8_t *)audio_buffer, bytes_read);

    if (ret != ESP_OK) {
      // Frame was dropped, already logged in send_audio_frame
      s_state.stats.frames_dropped++;
    }

    // Log statistics periodically
    log_stats();
  }

  ESP_LOGI(TAG, "Audio streaming task stopped");
  s_state.task_handle = NULL;
  vTaskDelete(NULL);
}

esp_err_t audio_streamer_init(ble_char_handle_t audio_data_handle) {
  if (s_state.initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Initializing audio streamer");

  // Initialize I2S audio with 16kHz sample rate
  esp_err_t ret = i2s_audio_init(SAMPLE_RATE_HZ);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2S: %s", esp_err_to_name(ret));
    return ret;
  }

  // Initialize state
  memset(&s_state, 0, sizeof(s_state));
  s_state.initialized = true;
  s_state.audio_data_handle = audio_data_handle;

  ESP_LOGI(TAG, "Audio streamer initialized (sample_rate=%d Hz)",
           SAMPLE_RATE_HZ);
  return ESP_OK;
}

esp_err_t audio_streamer_start(void) {
  if (!s_state.initialized) {
    ESP_LOGE(TAG, "Not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (s_state.running) {
    ESP_LOGW(TAG, "Already running");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Starting audio streaming task");

  s_state.running = true;

  BaseType_t ret =
      xTaskCreate(audio_streaming_task, "audio_stream", TASK_STACK_SIZE, NULL,
                  TASK_PRIORITY, &s_state.task_handle);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create streaming task");
    s_state.running = false;
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Audio streaming task started");
  return ESP_OK;
}

void audio_streamer_stop(void) {
  if (!s_state.running) {
    return;
  }

  ESP_LOGI(TAG, "Stopping audio streaming task");

  s_state.running = false;

  // Wait for task to finish (with timeout)
  int timeout_ms = 1000;
  while (s_state.task_handle != NULL && timeout_ms > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
    timeout_ms -= 10;
  }

  if (s_state.task_handle != NULL) {
    ESP_LOGW(TAG, "Task did not stop gracefully, force deleting");
    vTaskDelete(s_state.task_handle);
    s_state.task_handle = NULL;
  }

  ESP_LOGI(TAG, "Audio streaming task stopped");
}

void audio_streamer_deinit(void) {
  if (!s_state.initialized) {
    return;
  }

  if (s_state.running) {
    ESP_LOGW(TAG, "Stopping streaming before deinit");
    audio_streamer_stop();
  }

  ESP_LOGI(TAG, "Deinitializing audio streamer");

  i2s_audio_deinit();

  memset(&s_state, 0, sizeof(s_state));

  ESP_LOGI(TAG, "Audio streamer deinitialized");
}

esp_err_t audio_streamer_get_stats(audio_streamer_stats_t *stats) {
  if (stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!s_state.initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Update uptime before returning stats
  if (s_state.streaming_start_time > 0) {
    int64_t now = get_time_ms();
    s_state.stats.uptime_seconds =
        (uint32_t)((now - s_state.streaming_start_time) / 1000);
  }

  memcpy(stats, &s_state.stats, sizeof(audio_streamer_stats_t));
  return ESP_OK;
}

void audio_streamer_reset_stats(void) {
  memset(&s_state.stats, 0, sizeof(s_state.stats));
  s_state.sequence_number = 0;
  s_state.stats_last_log_time = get_time_ms();
  s_state.streaming_start_time = get_time_ms();
  ESP_LOGI(TAG, "Statistics reset");
}
