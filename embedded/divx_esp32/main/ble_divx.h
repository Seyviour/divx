
/**
 * @file ble_divx.h
 * @author your name (you@domain.com)
 * @brief bluetooth initialization for divx
 * @version 0.1
 * @date 2023-03-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#ifndef BLE_DIVX_H
#define BLE_DIVX_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"


#define DIVX_BLE_TAG "DIVX_BLE"

#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0
#define DIVX_APP_ID 0xAA
#define DEVICE_NAME "DIVX_DEMO_SCANNER"
#define SVC_INST_ID 0

/**
 * @brief The max length of characteristic value. When th eGATT client performas a write or prepare write operation,
 * the data length must be less than DIVX_CHAR_VAL_LEN_MAX
 * 
 */

#define DIVX_CHAR_VAL_LEN_MAX 20
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

#define ADV_CONFIG_FLAG         (1<<0)
#define SCAN_RSP_CONFIG_FLAG    (1<<1)


enum {
	DIVX_IDX_SVC,
	
	DIVX_IDX_XCODE_CHAR,
	DIVX_IDX_XCODE_VAL,
	DIVX_IDX_XCODE_NTF_CFG,
	
	DIVX_IDX_NB,
};

static uint8_t adv_config_done = 0;

uint16_t divx_handle_table[DIVX_IDX_NB]; 

static uint16_t divx_conn_id = 0;
static uint8_t service_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

/* The length of adv data must be less than 31 bytes */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = 0x00,
    .manufacturer_len    = 0,    //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //test_manufacturer,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};


// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
					esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static struct gatts_profile_inst divx_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};


//service
static const uint16_t GATTS_SERVICE_UUID_DIVX = 0X00FF;
static const uint16_t GATTS_CHAR_UUID_XCODE = 0xFF01;

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t characteristic_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t characteristic_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read                =  ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint8_t char_xcode_ccc[2]  =     {0x00, 0x00};
// static const uint8_t char_value[4]      =     {0x11, 0x22, 0x33, 0x44};
uint8_t xcode_char_value[16]      =     {0x11, 0x22, 0x33, 0x44};


/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[DIVX_IDX_NB] =
{
    // Service Declaration
    [DIVX_IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_DIVX), (uint8_t *)&GATTS_SERVICE_UUID_DIVX}},

    /* Characteristic Declaration */
    [DIVX_IDX_XCODE_CHAR]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

    /* Characteristic Value */
    [DIVX_IDX_XCODE_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_XCODE, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      DIVX_CHAR_VAL_LEN_MAX, sizeof(xcode_char_value), (uint8_t *)xcode_char_value}},

    /* Client Characteristic Configuration Descriptor */
    [DIVX_IDX_XCODE_NTF_CFG]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(char_xcode_ccc), (uint8_t *)char_xcode_ccc}},

};

// static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_add_duplicate_scan_exceptional_device *param){
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
    switch(event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;

        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(DIVX_BLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;

        default:
            break;
        
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:{
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
            if (set_dev_name_ret){
                ESP_LOGE(DIVX_BLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }
    
            //config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(DIVX_BLE_TAG, "config adv data failed, error code = %x", ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;
            //config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(DIVX_BLE_TAG, "config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
    #
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, DIVX_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(DIVX_BLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
       	    break;


        case ESP_GATTS_READ_EVT:
            //ESP_GATT_AUTO_RSP means the read is handled automatically. See attribute table
            ESP_LOGI(DIVX_BLE_TAG, "ESP_GATTS_READ_EVT");
       	    break;

        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep){
                // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
                ESP_LOGI(DIVX_BLE_TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
                esp_log_buffer_hex(DIVX_BLE_TAG, param->write.value, param->write.len);
                if (divx_handle_table[DIVX_IDX_XCODE_NTF_CFG] == param->write.handle && param->write.len == 2){
                    uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                    if (descr_value == 0x0001){
                        ESP_LOGI(DIVX_BLE_TAG, "notify enable");
                        // I guess this would take precedence over the automatic notifies
                        // Or at least get scheduled into the notify queue. Ditto for the indicate
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            // notify_data[i] = i % 0xff;
                            notify_data[i] = '.';
                        }
                        //the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, divx_handle_table[DIVX_IDX_XCODE_VAL],
                                                sizeof(notify_data), notify_data, false);
                    }else if (descr_value == 0x0002){
                        ESP_LOGI(DIVX_BLE_TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            // indicate_data[i] = i % 0xff;
                            indicate_data[i] = '.';
                        }
                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, divx_handle_table[DIVX_IDX_XCODE_VAL],
                                            sizeof(indicate_data), indicate_data, true);
                    }
                    else if (descr_value == 0x0000){
                        ESP_LOGI(DIVX_BLE_TAG, "notify/indicate disable ");
                    }else{
                        ESP_LOGE(DIVX_BLE_TAG, "unknown descr value");
                        esp_log_buffer_hex(DIVX_BLE_TAG, param->write.value, param->write.len);
                    }

                }
                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }else{
                /* handle prepare write */
                ESP_LOGI(DIVX_BLE_TAG, "Prepared writes not supported :)"); 
                // example_prepare_write_event_env(gatts_if, &prepare_write_env, param);
            }
      	    break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            // the length of gattc prepare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(DIVX_BLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            ESP_LOGI(DIVX_BLE_TAG, "Prepared writes not supported"); 
            // example_exec_write_event_env(&prepare_write_env, param);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(DIVX_BLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(DIVX_BLE_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(DIVX_BLE_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(DIVX_BLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            esp_log_buffer_hex(DIVX_BLE_TAG, param->connect.remote_bda, 6);
            divx_conn_id = param->connect.conn_id; 
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(DIVX_BLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(DIVX_BLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != DIVX_IDX_NB){
                ESP_LOGE(DIVX_BLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to DIVX_IDX_NB(%d)", param->add_attr_tab.num_handle, DIVX_IDX_NB);
            }
            else {
                ESP_LOGI(DIVX_BLE_TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(divx_handle_table, param->add_attr_tab.handles, sizeof(divx_handle_table));
                esp_ble_gatts_start_service(divx_handle_table[DIVX_IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            divx_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(DIVX_BLE_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == divx_profile_tab[idx].gatts_if) {
                if (divx_profile_tab[idx].gatts_cb) {
                    divx_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}


esp_err_t set_notify_xcode_value(uint8_t *xvalue){
    esp_err_t ret = esp_ble_gatts_set_attr_value(divx_handle_table[DIVX_IDX_XCODE_VAL], sizeof(xcode_char_value), xvalue);
    ret = esp_ble_gatts_send_indicate(divx_profile_tab[PROFILE_APP_IDX].gatts_if, divx_conn_id, divx_handle_table[DIVX_IDX_XCODE_VAL],
                                                sizeof(xcode_char_value), xvalue, false);
    return ret;
}


#endif