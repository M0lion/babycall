/**
 * @file ble_audio_stream.c
 * @brief BLE GATT Server Implementation for Baby Monitor Audio Streaming
 */

#include "ble_audio_stream.h"
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BLE_AUDIO";

// GATT Interface and Application IDs
#define GATTS_SERVICE_UUID_BABY_MONITOR   0x00FF
#define GATTS_NUM_HANDLE                  20
#define APP_ID                            0x55

// Connection parameters
#define BLE_CONN_INTERVAL_MIN             0x06   // 7.5ms
#define BLE_CONN_INTERVAL_MAX             0x0C   // 15ms
#define BLE_CONN_SLAVE_LATENCY            0x00   // No latency
#define BLE_CONN_SUPERVISION_TIMEOUT      0x0190 // 4s (400 * 10ms)
#define BLE_MTU_REQUEST_SIZE              512

// Advertisement data
#define DEVICE_NAME_PREFIX                "BabyMonitor-"
#define DEVICE_NAME_MAX_LEN               20

// Attribute handle indices
enum {
    IDX_SVC,

    IDX_AUDIO_DATA_CHAR,
    IDX_AUDIO_DATA_VAL,
    IDX_AUDIO_DATA_CFG,

    IDX_AUDIO_CONFIG_CHAR,
    IDX_AUDIO_CONFIG_VAL,

    IDX_CONTROL_CMD_CHAR,
    IDX_CONTROL_CMD_VAL,
    IDX_CONTROL_CMD_CFG,

    IDX_DEVICE_INFO_CHAR,
    IDX_DEVICE_INFO_VAL,

    IDX_NB,
};

// Service UUID (0x1900)
static const uint8_t service_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00
};

// Audio Data Characteristic UUID (0x1901)
static const uint8_t audio_data_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0x19, 0x00, 0x00
};

// Audio Config Characteristic UUID (0x1902)
static const uint8_t audio_config_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x00, 0x00
};

// Control Command Characteristic UUID (0x1903)
static const uint8_t control_cmd_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x03, 0x19, 0x00, 0x00
};

// Device Info Characteristic UUID (0x1904)
static const uint8_t device_info_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x04, 0x19, 0x00, 0x00
};

// BLE state structure
typedef struct {
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;
    uint16_t service_handle;
    uint16_t char_handle[IDX_NB];
    uint16_t mtu;
    bool connected;
    bool advertising;

    // Characteristic notification enable flags
    bool audio_data_notify_enabled;
    bool control_cmd_notify_enabled;

    // Current configurations
    ble_audio_config_t audio_config;
    ble_device_info_t device_info;

    // Callbacks
    ble_connection_cb_t connection_cb;
    ble_control_cmd_cb_t control_cmd_cb;
    ble_audio_config_cb_t audio_config_cb;

    char device_name[DEVICE_NAME_MAX_LEN];
} ble_audio_state_t;

static ble_audio_state_t ble_state = {
    .gatts_if = ESP_GATT_IF_NONE,
    .conn_id = 0xFFFF,
    .mtu = 23, // Default MTU
    .connected = false,
    .advertising = false,
    .audio_data_notify_enabled = false,
    .control_cmd_notify_enabled = false,
    .connection_cb = NULL,
    .control_cmd_cb = NULL,
    .audio_config_cb = NULL,
};

// Advertisement data
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = BLE_CONN_INTERVAL_MIN,
    .max_interval = BLE_CONN_INTERVAL_MAX,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = (uint8_t *)service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Advertisement parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Forward declarations
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

// Helper function to generate device name from MAC address
static void generate_device_name(char *name_buf, size_t buf_len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(name_buf, buf_len, "%s%02X%02X", DEVICE_NAME_PREFIX, mac[4], mac[5]);
}

// Helper function to set up GATT database
static esp_err_t setup_gatt_db(esp_gatt_if_t gatts_if) {
    esp_err_t ret;

    // Create service
    esp_gatt_srvc_id_t service_id = {
        .is_primary = true,
        .id.inst_id = 0x00,
        .id.uuid.len = ESP_UUID_LEN_128,
    };
    memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, sizeof(service_uuid));

    ret = esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create service: %d", ret);
        return ret;
    }

    return ESP_OK;
}

// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertisement data set complete");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising started successfully");
                ble_state.advertising = true;
            } else {
                ESP_LOGE(TAG, "Advertising start failed: %d", param->adv_start_cmpl.status);
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising stopped");
                ble_state.advertising = false;
            } else {
                ESP_LOGE(TAG, "Advertising stop failed: %d", param->adv_stop_cmpl.status);
            }
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "Connection params updated: status=%d, conn_int=%d, latency=%d, timeout=%d",
                     param->update_conn_params.status,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;

        default:
            break;
    }
}

// Handle characteristic read events
static void handle_read_event(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_gatt_rsp_t rsp;
    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
    rsp.attr_value.handle = param->read.handle;

    if (param->read.handle == ble_state.char_handle[IDX_AUDIO_CONFIG_VAL]) {
        // Read audio configuration
        rsp.attr_value.len = sizeof(ble_audio_config_t);
        memcpy(rsp.attr_value.value, &ble_state.audio_config, sizeof(ble_audio_config_t));
        ESP_LOGI(TAG, "Audio config read: rate=%d, bits=%d, ch=%d, status=%d",
                 ble_state.audio_config.sample_rate,
                 ble_state.audio_config.bit_depth,
                 ble_state.audio_config.channels,
                 ble_state.audio_config.status);
    } else if (param->read.handle == ble_state.char_handle[IDX_DEVICE_INFO_VAL]) {
        // Read device info
        rsp.attr_value.len = sizeof(ble_device_info_t);
        memcpy(rsp.attr_value.value, &ble_state.device_info, sizeof(ble_device_info_t));
        ESP_LOGI(TAG, "Device info read: fw=0x%08lX, batt=%d%%, uptime=%lus",
                 ble_state.device_info.fw_version,
                 ble_state.device_info.battery_level,
                 ble_state.device_info.uptime);
    }

    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                ESP_GATT_OK, &rsp);
}

// Handle characteristic write events
static void handle_write_event(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (param->write.handle == ble_state.char_handle[IDX_AUDIO_DATA_CFG]) {
        // Audio data notification enable/disable
        uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
        if (descr_value == 0x0001) {
            ESP_LOGI(TAG, "Audio data notifications enabled");
            ble_state.audio_data_notify_enabled = true;
        } else if (descr_value == 0x0000) {
            ESP_LOGI(TAG, "Audio data notifications disabled");
            ble_state.audio_data_notify_enabled = false;
        }
    } else if (param->write.handle == ble_state.char_handle[IDX_CONTROL_CMD_CFG]) {
        // Control command notification enable/disable
        uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
        if (descr_value == 0x0001) {
            ESP_LOGI(TAG, "Control command notifications enabled");
            ble_state.control_cmd_notify_enabled = true;
        } else if (descr_value == 0x0000) {
            ESP_LOGI(TAG, "Control command notifications disabled");
            ble_state.control_cmd_notify_enabled = false;
        }
    } else if (param->write.handle == ble_state.char_handle[IDX_AUDIO_CONFIG_VAL]) {
        // Audio configuration write
        if (param->write.len == sizeof(ble_audio_config_t)) {
            memcpy(&ble_state.audio_config, param->write.value, sizeof(ble_audio_config_t));
            ESP_LOGI(TAG, "Audio config written: rate=%d, bits=%d, ch=%d, status=%d",
                     ble_state.audio_config.sample_rate,
                     ble_state.audio_config.bit_depth,
                     ble_state.audio_config.channels,
                     ble_state.audio_config.status);

            // Call callback if registered
            if (ble_state.audio_config_cb) {
                ble_state.audio_config_cb(&ble_state.audio_config);
            }
        } else {
            ESP_LOGW(TAG, "Invalid audio config length: %d", param->write.len);
        }
    } else if (param->write.handle == ble_state.char_handle[IDX_CONTROL_CMD_VAL]) {
        // Control command write
        if (param->write.len >= 2) {
            uint8_t cmd_id = param->write.value[0];
            uint8_t params_len = param->write.value[1];
            const uint8_t *params = (params_len > 0) ? &param->write.value[2] : NULL;

            ESP_LOGI(TAG, "Control command received: cmd=0x%02X, params_len=%d", cmd_id, params_len);

            // Call callback if registered
            if (ble_state.control_cmd_cb) {
                ble_state.control_cmd_cb(cmd_id, params, params_len);
            }
        } else {
            ESP_LOGW(TAG, "Invalid control command length: %d", param->write.len);
        }
    }

    // Send write response if needed
    if (param->write.need_rsp) {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                    ESP_GATT_OK, NULL);
    }
}

// GATTS event handler
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATT server registered, app_id=%04x", param->reg.app_id);
            ble_state.gatts_if = gatts_if;

            // Set device name
            esp_ble_gap_set_device_name(ble_state.device_name);

            // Configure advertisement data
            esp_ble_gap_config_adv_data(&adv_data);

            // Setup GATT database
            setup_gatt_db(gatts_if);
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(TAG, "Service created, status=%d, service_handle=%d",
                     param->create.status, param->create.service_handle);

            if (param->create.status == ESP_GATT_OK) {
                ble_state.service_handle = param->create.service_handle;
                ble_state.char_handle[IDX_SVC] = param->create.service_handle;

                // Start service
                esp_ble_gatts_start_service(ble_state.service_handle);

                // Add Audio Data characteristic (NOTIFY)
                esp_bt_uuid_t audio_data_char_uuid = {
                    .len = ESP_UUID_LEN_128,
                };
                memcpy(audio_data_char_uuid.uuid.uuid128, audio_data_uuid, sizeof(audio_data_uuid));

                esp_ble_gatts_add_char(ble_state.service_handle, &audio_data_char_uuid,
                                      ESP_GATT_PERM_READ,
                                      ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                      NULL, NULL);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT: {
            ESP_LOGI(TAG, "Characteristic added, status=%d, attr_handle=%d, service_handle=%d",
                     param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);

            if (param->add_char.status == ESP_GATT_OK) {
                // Determine which characteristic was just added and add the next one
                if (ble_state.char_handle[IDX_AUDIO_DATA_CHAR] == 0) {
                    ble_state.char_handle[IDX_AUDIO_DATA_CHAR] = param->add_char.attr_handle;

                    // Add Audio Config characteristic (READ, WRITE)
                    esp_bt_uuid_t audio_config_char_uuid = {
                        .len = ESP_UUID_LEN_128,
                    };
                    memcpy(audio_config_char_uuid.uuid.uuid128, audio_config_uuid, sizeof(audio_config_uuid));

                    esp_ble_gatts_add_char(ble_state.service_handle, &audio_config_char_uuid,
                                          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                          ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                          NULL, NULL);
                } else if (ble_state.char_handle[IDX_AUDIO_CONFIG_CHAR] == 0) {
                    ble_state.char_handle[IDX_AUDIO_CONFIG_CHAR] = param->add_char.attr_handle;

                    // Add Control Command characteristic (WRITE, NOTIFY)
                    esp_bt_uuid_t control_cmd_char_uuid = {
                        .len = ESP_UUID_LEN_128,
                    };
                    memcpy(control_cmd_char_uuid.uuid.uuid128, control_cmd_uuid, sizeof(control_cmd_uuid));

                    esp_ble_gatts_add_char(ble_state.service_handle, &control_cmd_char_uuid,
                                          ESP_GATT_PERM_WRITE,
                                          ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                          NULL, NULL);
                } else if (ble_state.char_handle[IDX_CONTROL_CMD_CHAR] == 0) {
                    ble_state.char_handle[IDX_CONTROL_CMD_CHAR] = param->add_char.attr_handle;

                    // Add Device Info characteristic (READ)
                    esp_bt_uuid_t device_info_char_uuid = {
                        .len = ESP_UUID_LEN_128,
                    };
                    memcpy(device_info_char_uuid.uuid.uuid128, device_info_uuid, sizeof(device_info_uuid));

                    esp_ble_gatts_add_char(ble_state.service_handle, &device_info_char_uuid,
                                          ESP_GATT_PERM_READ,
                                          ESP_GATT_CHAR_PROP_BIT_READ,
                                          NULL, NULL);
                } else if (ble_state.char_handle[IDX_DEVICE_INFO_CHAR] == 0) {
                    ble_state.char_handle[IDX_DEVICE_INFO_CHAR] = param->add_char.attr_handle;
                    ESP_LOGI(TAG, "All characteristics added successfully");
                }
            }
            break;
        }

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            ESP_LOGI(TAG, "Descriptor added, status=%d, attr_handle=%d",
                     param->add_char_descr.status, param->add_char_descr.attr_handle);

            // Track descriptor handles
            if (ble_state.char_handle[IDX_AUDIO_DATA_CFG] == 0) {
                ble_state.char_handle[IDX_AUDIO_DATA_CFG] = param->add_char_descr.attr_handle;
            } else if (ble_state.char_handle[IDX_CONTROL_CMD_CFG] == 0) {
                ble_state.char_handle[IDX_CONTROL_CMD_CFG] = param->add_char_descr.attr_handle;
            }
            break;

        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Service started, status=%d, service_handle=%d",
                     param->start.status, param->start.service_handle);
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Client connected, conn_id=%d, remote " ESP_BD_ADDR_STR,
                     param->connect.conn_id,
                     ESP_BD_ADDR_HEX(param->connect.remote_bda));

            ble_state.conn_id = param->connect.conn_id;
            ble_state.connected = true;
            ble_state.advertising = false;

            // Update connection parameters
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.min_int = BLE_CONN_INTERVAL_MIN;
            conn_params.max_int = BLE_CONN_INTERVAL_MAX;
            conn_params.latency = BLE_CONN_SLAVE_LATENCY;
            conn_params.timeout = BLE_CONN_SUPERVISION_TIMEOUT;
            esp_ble_gap_update_conn_params(&conn_params);

            // Note: MTU exchange is initiated by the client. The server handles
            // it via ESP_GATTS_MTU_EVT. Local MTU is set during initialization.

            // Call connection callback
            if (ble_state.connection_cb) {
                ble_state.connection_cb(true);
            }
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Client disconnected, conn_id=%d, reason=0x%x",
                     param->disconnect.conn_id, param->disconnect.reason);

            ble_state.conn_id = 0xFFFF;
            ble_state.connected = false;
            ble_state.audio_data_notify_enabled = false;
            ble_state.control_cmd_notify_enabled = false;
            ble_state.mtu = 23; // Reset to default MTU

            // Call connection callback
            if (ble_state.connection_cb) {
                ble_state.connection_cb(false);
            }

            // Restart advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "Read request, conn_id=%d, trans_id=%ld, handle=%d",
                     param->read.conn_id, param->read.trans_id, param->read.handle);
            handle_read_event(gatts_if, param);
            break;

        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(TAG, "Write request, conn_id=%d, trans_id=%ld, handle=%d, len=%d",
                     param->write.conn_id, param->write.trans_id,
                     param->write.handle, param->write.len);
            handle_write_event(gatts_if, param);
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "MTU exchange, conn_id=%d, mtu=%d",
                     param->mtu.conn_id, param->mtu.mtu);
            ble_state.mtu = param->mtu.mtu;
            break;

        case ESP_GATTS_CONF_EVT:
            // Notification/Indication confirmation
            if (param->conf.status != ESP_GATT_OK) {
                ESP_LOGW(TAG, "Notification confirmation failed: %d", param->conf.status);
            }
            break;

        default:
            break;
    }
}

// Public API implementation

esp_err_t ble_audio_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing BLE audio streaming service...");

    // Generate device name
    generate_device_name(ble_state.device_name, sizeof(ble_state.device_name));
    ESP_LOGI(TAG, "Device name: %s", ble_state.device_name);

    // Initialize NVS (required for BLE)
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to release BT Classic memory: %s", esp_err_to_name(ret));
    }

    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BT controller: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable BT controller: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable Bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATTS callback: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register GATT application
    ret = esp_ble_gatts_app_register(APP_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATT app: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set local MTU
    ret = esp_ble_gatt_set_local_mtu(BLE_MTU_REQUEST_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set local MTU: %s", esp_err_to_name(ret));
    }

    // Initialize default configuration
    ble_state.audio_config.sample_rate = 16000;
    ble_state.audio_config.bit_depth = 16;
    ble_state.audio_config.channels = 1;
    ble_state.audio_config.status = 0; // Stopped

    // Initialize default device info
    ble_state.device_info.fw_version = 0x00010000; // v1.0.0
    ble_state.device_info.battery_level = 100;
    ble_state.device_info.uptime = 0;

    ESP_LOGI(TAG, "BLE audio service initialized successfully");
    return ESP_OK;
}

void ble_audio_start_advertising(void) {
    if (!ble_state.advertising && !ble_state.connected) {
        ESP_LOGI(TAG, "Starting advertising...");
        esp_ble_gap_start_advertising(&adv_params);
    } else {
        ESP_LOGW(TAG, "Already advertising or connected");
    }
}

void ble_audio_stop_advertising(void) {
    if (ble_state.advertising) {
        ESP_LOGI(TAG, "Stopping advertising...");
        esp_ble_gap_stop_advertising();
    }
}

esp_err_t ble_audio_send_frame(const uint8_t *data, size_t len) {
    if (!ble_state.connected) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ble_state.audio_data_notify_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    if (len > (ble_state.mtu - 3)) {
        ESP_LOGW(TAG, "Audio frame too large for MTU: %zu > %d", len, ble_state.mtu - 3);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = esp_ble_gatts_send_indicate(ble_state.gatts_if, ble_state.conn_id,
                                                ble_state.char_handle[IDX_AUDIO_DATA_CHAR],
                                                len, (uint8_t *)data, false);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send audio frame: %s", esp_err_to_name(ret));
    }

    return ret;
}

bool ble_audio_is_connected(void) {
    return ble_state.connected;
}

uint16_t ble_audio_get_mtu(void) {
    return ble_state.mtu;
}

void ble_audio_set_connection_callback(ble_connection_cb_t callback) {
    ble_state.connection_cb = callback;
}

void ble_audio_set_control_callback(ble_control_cmd_cb_t callback) {
    ble_state.control_cmd_cb = callback;
}

void ble_audio_set_config_callback(ble_audio_config_cb_t callback) {
    ble_state.audio_config_cb = callback;
}

esp_err_t ble_audio_update_config(const ble_audio_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&ble_state.audio_config, config, sizeof(ble_audio_config_t));

    ESP_LOGI(TAG, "Audio config updated: rate=%d, bits=%d, ch=%d, status=%d",
             config->sample_rate, config->bit_depth, config->channels, config->status);

    return ESP_OK;
}

esp_err_t ble_audio_update_device_info(const ble_device_info_t *info) {
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&ble_state.device_info, info, sizeof(ble_device_info_t));

    ESP_LOGI(TAG, "Device info updated: fw=0x%08lX, batt=%d%%, uptime=%lus",
             info->fw_version, info->battery_level, info->uptime);

    return ESP_OK;
}

esp_err_t ble_audio_send_control_response(uint8_t cmd_id, uint8_t status) {
    if (!ble_state.connected) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ble_state.control_cmd_notify_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    // Response format: [cmd_id(1B), status(1B)]
    uint8_t response[2] = {cmd_id, status};

    esp_err_t ret = esp_ble_gatts_send_indicate(ble_state.gatts_if, ble_state.conn_id,
                                                ble_state.char_handle[IDX_CONTROL_CMD_CHAR],
                                                sizeof(response), response, false);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send control response: %s", esp_err_to_name(ret));
    }

    return ret;
}
