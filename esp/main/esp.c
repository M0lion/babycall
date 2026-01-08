/**
 * @file esp.c
 * @brief Baby Monitor Main Application
 *
 * This is the main application entry point for the ESP32-C3 baby monitor.
 * It initializes all required components and manages the audio streaming
 * pipeline over BLE.
 *
 * Architecture:
 * - NVS: Required for BLE stack operation
 * - BLE Audio Service: GATT server for audio streaming
 * - Audio Streamer: Manages I2S capture and BLE transmission
 *
 * @version 1.0.0
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include <stdio.h>

#include "ble/ble_simple.h"

#define LED_GPIO 8

static const char *TAG = "BABY_MONITOR";

// Firmware version: v1.0.0
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
#define FW_VERSION                                                             \
  ((FW_VERSION_MAJOR << 16) | (FW_VERSION_MINOR << 8) | FW_VERSION_PATCH)

/**
 * @brief Initialize NVS (Non-Volatile Storage)
 *
 * NVS is required by the BLE stack to store pairing information and
 * other persistent data. This function initializes NVS and handles
 * error recovery.
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t init_nvs(void) {
  ESP_LOGI(TAG, "Initializing NVS...");

  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition is full or has incompatible version
    // Erase and retry initialization
    ESP_LOGW(TAG, "NVS partition needs to be erased (reason: %s)",
             esp_err_to_name(ret));
    ESP_LOGI(TAG, "Erasing NVS partition...");

    ESP_ERROR_CHECK(nvs_flash_erase());

    ESP_LOGI(TAG, "Retrying NVS initialization...");
    ret = nvs_flash_init();
  }

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "NVS initialized successfully");
  } else {
    ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
  }

  return ret;
}

// Custom service UUID (generate your own!)
static const uint8_t SERVICE_UUID[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
                                         0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78,
                                         0x9A, 0xBC, 0xDE, 0xF0};

// Characteristic UUIDs
static const uint8_t TEMP_UUID[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
                                      0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78,
                                      0x9A, 0xBC, 0x00, 0x01};

static const uint8_t LED_UUID[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
                                     0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78,
                                     0x9A, 0xBC, 0x00, 0x02};

// Handle for sending notifications
static ble_char_handle_t temp_handle;

// Simulated sensor value
static float temperature = 25.0f;
static bool led_state = false;

// ============ Callbacks ============

// Called when client reads temperature
static void on_temp_read(ble_char_handle_t handle, uint8_t *out_data,
                         uint16_t *out_len) {
  // Just send the float as bytes
  memcpy(out_data, &temperature, sizeof(temperature));
  *out_len = sizeof(temperature);
  ESP_LOGI(TAG, "Temperature read: %.2f", temperature);
}

static led_strip_handle_t led_strip;
void configure_led(void) {
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_GPIO,
      .max_leds = 1,
  };

  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
      .flags.with_dma = false,
  };

  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  led_strip_clear(led_strip);
}

// Called when client writes to LED characteristic
static void on_led_write(ble_char_handle_t handle, const uint8_t *data,
                         uint16_t len) {
  if (len >= 1) {
    led_state = data[0] != 0;
    ESP_LOGI(TAG, "LED set to: %s", led_state ? "ON" : "OFF");
    if (led_state) {
      led_strip_set_pixel(led_strip, 0, 0, 50, 0);
      led_strip_refresh(led_strip);
    } else {
      led_strip_clear(led_strip);
    }
  }
}

// Called when client reads LED state
static void on_led_read(ble_char_handle_t handle, uint8_t *out_data,
                        uint16_t *out_len) {
  out_data[0] = led_state ? 1 : 0;
  *out_len = 1;
}

// Called on connect/disconnect
static void on_connection_change(bool connected) {
  ESP_LOGI(TAG, "Connection state: %s",
           connected ? "CONNECTED" : "DISCONNECTED");
}

/**
 * @brief Main application entry point
 *
 * Initializes all components in the correct order and starts the
 * baby monitor audio streaming service.
 */
void app_main(void) {
  esp_err_t ret;

  // Step 1: Initialize NVS (required for BLE)
  ret = init_nvs();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "NVS initialization failed - cannot continue");
    return;
  }

  configure_led();

  // Initialize simple BLE
  ble_simple_config_t config = {
      .device_name = "MyDevice",
      .on_connect = on_connection_change,
  };
  ESP_ERROR_CHECK(ble_simple_init(&config));

  // Add service
  int service_idx;
  ESP_ERROR_CHECK(ble_simple_add_service(SERVICE_UUID, &service_idx));

  // Temperature: readable + notifications
  ble_char_config_t temp_config = {.on_read = on_temp_read,
                                   .on_write = NULL, // Not writable
                                   .notifications = true};
  memcpy(temp_config.uuid, TEMP_UUID, 16);
  ESP_ERROR_CHECK(
      ble_simple_add_characteristic(service_idx, &temp_config, &temp_handle));

  // LED: readable + writable, no notifications
  ble_char_config_t led_config = {
      .on_read = on_led_read, .on_write = on_led_write, .notifications = false};
  memcpy(led_config.uuid, LED_UUID, 16);
  ESP_ERROR_CHECK(
      ble_simple_add_characteristic(service_idx, &led_config, NULL));

  // 4. Start advertising
  ESP_ERROR_CHECK(ble_simple_start_advertising());
  ESP_LOGI(TAG, "BLE ready! Advertising as 'MyDevice'");

  // 5. Main loop - send temperature notifications
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Simulate temperature change
    temperature += 0.1f;
    if (temperature > 30.0f)
      temperature = 20.0f;

    // Send notification if connected
    if (ble_simple_is_connected()) {
      esp_err_t ret = ble_simple_notify(temp_handle, (uint8_t *)&temperature,
                                        sizeof(temperature));
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sent temp notification: %.2f", temperature);
      }
    }
  }
}
