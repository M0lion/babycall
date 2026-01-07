#pragma once

#include "esp_http_server.h"

httpd_handle_t audio_stream_start(void);
void audio_stream_stop(httpd_handle_t server);
