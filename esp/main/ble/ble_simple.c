#include "ble_simple.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ble_simple";

// Internal characteristic structure
typedef struct ble_char_handle {
  uint16_t attr_handle;
  uint16_t cccd_handle; // Client Characteristic Config Descriptor
  uint8_t uuid[16];
  ble_read_cb_t on_read;
  ble_write_cb_t on_write;
  bool notifications;
  bool indicate;
  bool notify_enabled;
  uint16_t max_len;
  int service_idx;
} ble_char_t;

// Internal service structure
typedef struct {
  uint8_t uuid[16];
  uint16_t handle;
  int char_count;
  bool created;
} ble_service_t;

// Module state
static struct {
  bool initialized;
  bool advertising;
  uint16_t conn_id;
  bool connected;
  esp_gatt_if_t gatts_if;
  ble_connect_cb_t on_connect;
  char device_name[32];

  ble_service_t services[BLE_SIMPLE_MAX_SERVICES];
  int service_count;

  ble_char_t characteristics[BLE_SIMPLE_MAX_CHARACTERISTICS];
  int char_count;

  int current_service_setup; // Track which service we're setting up
} state = {0};

// Advertising data
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true, // This is a scan response
    .include_name = false,
    .include_txpower = false,
    .service_uuid_len = 16, // Put UUID here
    .p_service_uuid = NULL, // Will be set at runtime
    .flag = 0,
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Find characteristic by attribute handle
static ble_char_t *find_char_by_handle(uint16_t handle) {
  for (int i = 0; i < state.char_count; i++) {
    if (state.characteristics[i].attr_handle == handle ||
        state.characteristics[i].cccd_handle == handle) {
      return &state.characteristics[i];
    }
  }
  return NULL;
}

// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    ESP_LOGI(TAG, "Advertising data set complete");
    if (state.advertising) {
      esp_ble_gap_start_advertising(&adv_params);
    }
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
      ESP_LOGI(TAG, "Advertising started");
    } else {
      ESP_LOGE(TAG, "Advertising start failed: %d",
               param->adv_start_cmpl.status);
    }
    break;

  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    ESP_LOGI(TAG, "Advertising stopped");
    break;

  case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    ESP_LOGI(TAG, "Connection params updated");
    break;

  default:
    break;
  }
}

// Create attribute table for a service
static void create_service_attrs(int service_idx) {
  ble_service_t *svc = &state.services[service_idx];

  // Count attributes: 1 for service + (3 per char: decl, value, cccd if notify)
  int attr_count = 1;
  for (int i = 0; i < state.char_count; i++) {
    if (state.characteristics[i].service_idx == service_idx) {
      attr_count += 2; // Declaration + Value
      if (state.characteristics[i].notifications ||
          state.characteristics[i].indicate) {
        attr_count++; // CCCD
      }
    }
  }

  // Build attribute table dynamically
  esp_gatts_attr_db_t *attr_db =
      calloc(attr_count, sizeof(esp_gatts_attr_db_t));
  if (!attr_db) {
    ESP_LOGE(TAG, "Failed to allocate attribute table");
    return;
  }

  int idx = 0;

  // Primary service declaration
  static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
  attr_db[idx].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
  attr_db[idx].att_desc.uuid_length = sizeof(uint16_t);
  attr_db[idx].att_desc.uuid_p = (uint8_t *)&primary_service_uuid;
  attr_db[idx].att_desc.perm = ESP_GATT_PERM_READ;
  attr_db[idx].att_desc.max_length = sizeof(svc->uuid);
  attr_db[idx].att_desc.length = sizeof(svc->uuid);
  attr_db[idx].att_desc.value = svc->uuid;
  idx++;

  // Add characteristics
  static const uint16_t char_decl_uuid = ESP_GATT_UUID_CHAR_DECLARE;
  static const uint16_t cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

  for (int i = 0; i < state.char_count; i++) {
    ble_char_t *chr = &state.characteristics[i];
    if (chr->service_idx != service_idx)
      continue;

    // Build properties byte
    uint8_t props = 0;
    if (chr->on_read)
      props |= ESP_GATT_CHAR_PROP_BIT_READ;
    if (chr->on_write)
      props |= ESP_GATT_CHAR_PROP_BIT_WRITE;
    if (chr->notifications)
      props |= ESP_GATT_CHAR_PROP_BIT_NOTIFY;
    if (chr->indicate)
      props |= ESP_GATT_CHAR_PROP_BIT_INDICATE;

    // Store properties in the characteristic for later reference
    static uint8_t prop_storage[BLE_SIMPLE_MAX_CHARACTERISTICS];
    prop_storage[i] = props;

    // Characteristic declaration
    attr_db[idx].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
    attr_db[idx].att_desc.uuid_length = sizeof(uint16_t);
    attr_db[idx].att_desc.uuid_p = (uint8_t *)&char_decl_uuid;
    attr_db[idx].att_desc.perm = ESP_GATT_PERM_READ;
    attr_db[idx].att_desc.max_length = sizeof(uint8_t);
    attr_db[idx].att_desc.length = sizeof(uint8_t);
    attr_db[idx].att_desc.value = &prop_storage[i];
    idx++;

    // Characteristic value
    esp_gatt_perm_t perm = 0;
    if (chr->on_read)
      perm |= ESP_GATT_PERM_READ;
    if (chr->on_write)
      perm |= ESP_GATT_PERM_WRITE;

    attr_db[idx].attr_control.auto_rsp =
        ESP_GATT_RSP_BY_APP; // We handle responses
    attr_db[idx].att_desc.uuid_length = sizeof(chr->uuid);
    attr_db[idx].att_desc.uuid_p = chr->uuid;
    attr_db[idx].att_desc.perm = perm;
    attr_db[idx].att_desc.max_length = chr->max_len;
    attr_db[idx].att_desc.length = 0;
    attr_db[idx].att_desc.value = NULL;
    idx++;

    // CCCD for notifications/indications
    if (chr->notifications || chr->indicate) {
      static uint8_t cccd_value[2] = {0, 0};
      attr_db[idx].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
      attr_db[idx].att_desc.uuid_length = sizeof(uint16_t);
      attr_db[idx].att_desc.uuid_p = (uint8_t *)&cccd_uuid;
      attr_db[idx].att_desc.perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
      attr_db[idx].att_desc.max_length = sizeof(cccd_value);
      attr_db[idx].att_desc.length = sizeof(cccd_value);
      attr_db[idx].att_desc.value = cccd_value;
      idx++;
    }
  }

  esp_err_t ret = esp_ble_gatts_create_attr_tab(attr_db, state.gatts_if,
                                                attr_count, service_idx);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Create attr table failed: %s", esp_err_to_name(ret));
  }

  free(attr_db);
}

// GATTS event handler
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
  switch (event) {
  case ESP_GATTS_REG_EVT:
    if (param->reg.status == ESP_GATT_OK) {
      state.gatts_if = gatts_if;
      ESP_LOGI(TAG, "GATT server registered, app_id=%d", param->reg.app_id);

      // Set device name
      esp_ble_gap_set_device_name(state.device_name);

      if (state.service_count > 0) {
        // Use the first service UUID for advertising
        scan_rsp_data.service_uuid_len = 16; // 128-bit UUID
        scan_rsp_data.p_service_uuid = state.services[0].uuid;
      }

      // Configure advertising data
      esp_ble_gap_config_adv_data(&adv_data);
      esp_ble_gap_config_adv_data(&scan_rsp_data);

      // Create first service if any exist
      if (state.service_count > 0) {
        state.current_service_setup = 0;
        create_service_attrs(0);
      }
    } else {
      ESP_LOGE(TAG, "GATT register failed: %d", param->reg.status);
    }
    break;

  case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    if (param->add_attr_tab.status == ESP_GATT_OK) {
      int svc_idx = param->add_attr_tab.svc_inst_id;
      ESP_LOGI(TAG, "Attribute table created for service %d, handles=%d",
               svc_idx, param->add_attr_tab.num_handle);

      // Store handles
      state.services[svc_idx].handle = param->add_attr_tab.handles[0];

      // Map handles to characteristics
      int handle_idx = 1;
      for (int i = 0; i < state.char_count; i++) {
        ble_char_t *chr = &state.characteristics[i];
        if (chr->service_idx != svc_idx)
          continue;

        handle_idx++; // Skip declaration
        chr->attr_handle = param->add_attr_tab.handles[handle_idx++];

        if (chr->notifications || chr->indicate) {
          chr->cccd_handle = param->add_attr_tab.handles[handle_idx++];
        }

        ESP_LOGI(TAG, "Char handle=%d, cccd=%d", chr->attr_handle,
                 chr->cccd_handle);
      }

      // Start service
      esp_ble_gatts_start_service(state.services[svc_idx].handle);
    }
    break;

  case ESP_GATTS_START_EVT:
    if (param->start.status == ESP_GATT_OK) {
      ESP_LOGI(TAG, "Service started");

      // Create next service if any
      state.current_service_setup++;
      if (state.current_service_setup < state.service_count) {
        create_service_attrs(state.current_service_setup);
      }
    }
    break;

  case ESP_GATTS_CONNECT_EVT:
    ESP_LOGI(TAG, "Client connected, conn_id=%d", param->connect.conn_id);
    state.conn_id = param->connect.conn_id;
    state.connected = true;

    // Update connection parameters for better performance
    esp_ble_conn_update_params_t conn_params = {0};
    memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
    conn_params.latency = 0;
    conn_params.max_int = 0x20;
    conn_params.min_int = 0x10;
    conn_params.timeout = 400;
    esp_ble_gap_update_conn_params(&conn_params);

    if (state.on_connect) {
      state.on_connect(true);
    }
    break;

  case ESP_GATTS_DISCONNECT_EVT:
    ESP_LOGI(TAG, "Client disconnected, reason=0x%x", param->disconnect.reason);
    state.connected = false;

    // Reset notification state
    for (int i = 0; i < state.char_count; i++) {
      state.characteristics[i].notify_enabled = false;
    }

    if (state.on_connect) {
      state.on_connect(false);
    }

    // Restart advertising
    if (state.advertising) {
      esp_ble_gap_start_advertising(&adv_params);
    }
    break;

  case ESP_GATTS_READ_EVT: {
    ESP_LOGD(TAG, "Read request, handle=%d", param->read.handle);
    ble_char_t *chr = find_char_by_handle(param->read.handle);

    esp_gatt_rsp_t rsp = {0};
    rsp.attr_value.handle = param->read.handle;

    if (chr && chr->on_read && param->read.handle == chr->attr_handle) {
      chr->on_read((ble_char_handle_t)chr, rsp.attr_value.value,
                   &rsp.attr_value.len);
    }

    esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                param->read.trans_id, ESP_GATT_OK, &rsp);
    break;
  }

  case ESP_GATTS_WRITE_EVT: {
    ESP_LOGD(TAG, "Write request, handle=%d, len=%d", param->write.handle,
             param->write.len);

    ble_char_t *chr = find_char_by_handle(param->write.handle);

    if (chr) {
      // Check if this is a CCCD write (notification enable/disable)
      if (param->write.handle == chr->cccd_handle) {
        if (param->write.len == 2) {
          uint16_t cccd_value =
              param->write.value[0] | (param->write.value[1] << 8);
          chr->notify_enabled = (cccd_value & 0x0003) != 0;
          ESP_LOGI(TAG, "Notifications %s for handle %d",
                   chr->notify_enabled ? "enabled" : "disabled",
                   chr->attr_handle);
        }
      } else if (chr->on_write) {
        chr->on_write((ble_char_handle_t)chr, param->write.value,
                      param->write.len);
      }
    }

    if (param->write.need_rsp) {
      esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                  param->write.trans_id, ESP_GATT_OK, NULL);
    }
    break;
  }

  case ESP_GATTS_MTU_EVT:
    ESP_LOGI(TAG, "MTU updated to %d", param->mtu.mtu);
    break;

  default:
    break;
  }
}

esp_err_t ble_simple_init(const ble_simple_config_t *config) {
  if (state.initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (!config || !config->device_name) {
    return ESP_ERR_INVALID_ARG;
  }

  // Store config
  strncpy(state.device_name, config->device_name,
          sizeof(state.device_name) - 1);
  state.on_connect = config->on_connect;

  // Release classic BT memory
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  // Initialize BT controller
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_err_t ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Initialize Bluedroid
  ret = esp_bluedroid_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_bluedroid_enable();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Register callbacks
  ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "GATTS callback register failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_ble_gap_register_callback(gap_event_handler);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Set MTU
  esp_ble_gatt_set_local_mtu(517);

  state.initialized = true;
  ESP_LOGI(TAG, "BLE stack initialized: %s", state.device_name);
  ESP_LOGI(
      TAG,
      "Add services/characteristics, then call ble_simple_start_advertising()");

  return ESP_OK;
}

esp_err_t ble_simple_add_service(const uint8_t uuid[16], int *out_service_idx) {
  if (!state.initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (state.service_count >= BLE_SIMPLE_MAX_SERVICES) {
    return ESP_ERR_NO_MEM;
  }

  int idx = state.service_count++;
  memcpy(state.services[idx].uuid, uuid, 16);
  state.services[idx].created = false;

  if (out_service_idx) {
    *out_service_idx = idx;
  }

  ESP_LOGI(TAG, "Service added at index %d", idx);
  return ESP_OK;
}

esp_err_t ble_simple_add_characteristic(int service_idx,
                                        const ble_char_config_t *config,
                                        ble_char_handle_t *out_handle) {
  if (!state.initialized || !config) {
    return ESP_ERR_INVALID_ARG;
  }

  if (service_idx < 0 || service_idx >= state.service_count) {
    return ESP_ERR_INVALID_ARG;
  }

  if (state.char_count >= BLE_SIMPLE_MAX_CHARACTERISTICS) {
    return ESP_ERR_NO_MEM;
  }

  int idx = state.char_count++;
  ble_char_t *chr = &state.characteristics[idx];

  memcpy(chr->uuid, config->uuid, 16);
  chr->on_read = config->on_read;
  chr->on_write = config->on_write;
  chr->notifications = config->notifications;
  chr->indicate = config->indicate;
  chr->max_len =
      config->max_len > 0 ? config->max_len : BLE_SIMPLE_MAX_CHAR_VALUE_LEN;
  chr->service_idx = service_idx;
  chr->notify_enabled = false;

  state.services[service_idx].char_count++;

  if (out_handle) {
    *out_handle = (ble_char_handle_t)chr;
  }

  ESP_LOGI(TAG, "Characteristic added to service %d", service_idx);
  return ESP_OK;
}

esp_err_t ble_simple_start_advertising(void) {
  if (!state.initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  state.advertising = true;

  // If GATT app not yet registered, register it now
  // This triggers ESP_GATTS_REG_EVT which will create services and start
  // advertising
  if (state.gatts_if == 0) {
    ESP_LOGI(TAG, "Registering GATT application...");
    esp_err_t ret = esp_ble_gatts_app_register(0);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
      return ret;
    }
    // Advertising will start after GATT setup completes (in the event handler)
    return ESP_OK;
  }

  // GATT already set up, start advertising immediately
  return esp_ble_gap_start_advertising(&adv_params);
}

esp_err_t ble_simple_stop_advertising(void) {
  state.advertising = false;
  return esp_ble_gap_stop_advertising();
}

esp_err_t ble_simple_notify(ble_char_handle_t handle, const uint8_t *data,
                            uint16_t len) {
  if (!state.connected || !handle) {
    return ESP_ERR_INVALID_STATE;
  }

  ble_char_t *chr = (ble_char_t *)handle;

  if (!chr->notify_enabled) {
    return ESP_ERR_INVALID_STATE;
  }

  bool need_confirm = chr->indicate;
  return esp_ble_gatts_send_indicate(state.gatts_if, state.conn_id,
                                     chr->attr_handle, len, (uint8_t *)data,
                                     need_confirm);
}

bool ble_simple_is_connected(void) { return state.connected; }

esp_err_t ble_simple_deinit(void) {
  if (!state.initialized) {
    return ESP_OK;
  }

  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  memset(&state, 0, sizeof(state));

  return ESP_OK;
}
