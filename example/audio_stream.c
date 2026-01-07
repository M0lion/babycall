#include "audio_stream.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "audio_stream";

#define I2S_SCK_PIN 6
#define I2S_WS_PIN 5
#define I2S_SD_PIN 4

#define SAMPLE_RATE 16000

static i2s_chan_handle_t rx_chan = NULL;
static httpd_handle_t server = NULL;
static int ws_fd = -1;

static const char *html_page =
    "<!DOCTYPE html><html><body>"
    "<h1>ESP32 Audio Stream</h1>"
    "<button onclick='start()'>Start</button>"
    "<button onclick='stop()'>Stop</button>"
    "<p id='status'>Click Start</p>"
    "<script>"
    "let ws, ctx, nextTime = 0;"
    "function start() {"
    "  ctx = new AudioContext({sampleRate: 16000});"
    "  ws = new WebSocket('ws://' + location.host + '/ws');"
    "  ws.binaryType = 'arraybuffer';"
    "  ws.onopen = () => document.getElementById('status').textContent = "
    "'Connected';"
    "  ws.onclose = () => document.getElementById('status').textContent = "
    "'Disconnected';"
    "  ws.onmessage = (e) => {"
    "    const int16 = new Int16Array(e.data);"
    "    const float32 = new Float32Array(int16.length);"
    "    for (let i = 0; i < int16.length; i++) float32[i] = int16[i] / 32768;"
    "    const buf = ctx.createBuffer(1, float32.length, 16000);"
    "    buf.getChannelData(0).set(float32);"
    "    const src = ctx.createBufferSource();"
    "    src.buffer = buf;"
    "    src.connect(ctx.destination);"
    "    const now = ctx.currentTime;"
    "    if (nextTime < now) nextTime = now;"
    "    src.start(nextTime);"
    "    nextTime += buf.duration;"
    "  };"
    "}"
    "function stop() { if (ws) ws.close(); if (ctx) ctx.close(); }"
    "</script></body></html>";

static void i2s_init(void) {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 4;
  chan_cfg.dma_frame_num = 256;
  i2s_new_channel(&chan_cfg, NULL, &rx_chan);

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
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
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  i2s_channel_init_std_mode(rx_chan, &std_cfg);
  i2s_channel_enable(rx_chan);

  ESP_LOGI(TAG, "I2S initialized");
}

static void audio_task(void *arg) {
  int32_t samples_32[128];
  int16_t samples_16[128];
  size_t bytes_read;

  while (1) {
    int fd = ws_fd;
    if (fd < 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    esp_err_t ret = i2s_channel_read(rx_chan, samples_32, sizeof(samples_32),
                                     &bytes_read, pdMS_TO_TICKS(100));
    if (ret != ESP_OK || bytes_read == 0)
      continue;

    int num_samples = bytes_read / sizeof(int32_t);
    for (int i = 0; i < num_samples; i++) {
      samples_16[i] = samples_32[i] >> 16;
    }

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t *)samples_16,
        .len = num_samples * sizeof(int16_t),
    };

    if (httpd_ws_send_frame_async(server, fd, &ws_pkt) != ESP_OK) {
      ESP_LOGW(TAG, "Send failed, closing connection");
      ws_fd = -1;
      httpd_sess_trigger_close(server, fd); // Force close the socket
      vTaskDelay(
          pdMS_TO_TICKS(500)); // Brief pause before accepting new connections
    }
  }
}

static esp_err_t ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    // Handshake - save socket here
    ws_fd = httpd_req_to_sockfd(req);
    ESP_LOGI(TAG, "WebSocket client connected: %d", ws_fd);
    return ESP_OK;
  }

  // Handle incoming frames (we don't expect any, but just in case)
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;
  httpd_ws_recv_frame(req, &ws_pkt, 0);

  return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html_page, strlen(html_page));
  return ESP_OK;
}

static void on_ws_close(httpd_handle_t hd, int sockfd) {
  if (sockfd == ws_fd) {
    ESP_LOGI(TAG, "WebSocket client disconnected");
    ws_fd = -1;
  }
}

httpd_handle_t audio_stream_start(void) {
  i2s_init();

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.close_fn = on_ws_close;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t root = {
        .uri = "/", .method = HTTP_GET, .handler = root_handler};
    httpd_uri_t ws = {.uri = "/ws",
                      .method = HTTP_GET,
                      .handler = ws_handler,
                      .is_websocket = true};
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &ws);

    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "WebSocket audio server started");
  }
  return server;
}

void audio_stream_stop(httpd_handle_t srv) {
  if (srv) {
    httpd_stop(srv);
  }
  if (rx_chan) {
    i2s_channel_disable(rx_chan);
    i2s_del_channel(rx_chan);
  }
}
