#include "audio_stream.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include <string.h>

// Configure these for your access point
#define ESP_WIFI_SSID "MyESP32-AP"
#define ESP_WIFI_PASS "mypassword"
#define ESP_WIFI_CHANNEL 1
#define MAX_STA_CONN 4

static const char *TAG = "wifi_softAP";

// Event handler for WiFi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event =
        (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac),
             event->aid);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event =
        (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d, reason=%d",
             MAC2STR(event->mac), event->aid, event->reason);
  }
}

void wifi_init_softap(void) {
  // Initialize the network interface
  ESP_ERROR_CHECK(esp_netif_init());

  // Create default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Create default WiFi AP network interface
  esp_netif_create_default_wifi_ap();

  // Initialize WiFi with default configuration
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Register event handler
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

  // Configure AP settings
  wifi_config_t wifi_config = {
      .ap =
          {
              .ssid = ESP_WIFI_SSID,
              .ssid_len = strlen(ESP_WIFI_SSID),
              .channel = ESP_WIFI_CHANNEL,
              .password = ESP_WIFI_PASS,
              .max_connection = MAX_STA_CONN,
              .authmode = WIFI_AUTH_WPA2_PSK,
              .pmf_cfg =
                  {
                      .required = true,
                  },
          },
  };

  // Use open auth if no password
  if (strlen(ESP_WIFI_PASS) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  // Set mode to AP and apply configuration
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

  // Start WiFi
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "SoftAP started. SSID:%s password:%s channel:%d", ESP_WIFI_SSID,
           ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
  wifi_init_softap();
  audio_stream_start();
}
