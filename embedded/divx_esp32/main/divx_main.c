/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// modified from the Espressif continuous read adc example.


#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "ble_divx.h"
#include "sig_decoding.h"
#include "driver/gpio.h"
#include "esp_timer.h"



#define TAG "DIVX_MAIN"

 
#define READ_LEN   256      //DMA BUFFER SIZE
#define GET_UNIT(x)        ((x>>3) & 0x1)

//ADC RELATED
#define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1  //ESP32 only supports ADC1 DMA mode
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE1
static adc_channel_t channel[1] = {ADC_CHANNEL_6};

// Push button input pin
#define SCAN_RESET_PIN GPIO_NUM_33

static TaskHandle_t s_task_handle; // main task 
static TaskHandle_t button_start_task_handle = NULL; // Button decode start task 



/**
 * @brief 
 * 
 * @param x_df : A decode frame of type decode_frame_t
 * @param adc_stream : Byte array of adc readings
 * @param ret_num : number of valid samples in adc_stream
 */
void x_decode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num){
    // x_df -> x_code decode frame 
    incrementalDecode(x_df, adc_stream, ret_num); 

}

/**
 * @brief ADC call back function for when x(not sure of the number) number of conversions has been done
 * 
 * @param handle 
 * @param edata 
 * @param user_data 
 * @return true 
 * @return false 
 */
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

/**
 * @brief Initialize the ADC to work in continuous mode
 * 
 * @param channel : 
 * @param channel_num 
 * @param out_handle 
 */
static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 30 * 1000,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        uint8_t unit = GET_UNIT(channel[i]);
        uint8_t ch = channel[i] & 0x7;
        adc_pattern[i].atten = ADC_ATTEN_DB_11; // measure up to 3.3v
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

#if !CONFIG_IDF_TARGET_ESP32
static bool check_valid_data(const adc_digi_output_data_t *data)
{
    const unsigned int unit = data->type2.unit;
    if (unit > 2) return false;
    if (data->type2.channel >= SOC_ADC_CHANNEL_NUM(unit)) return false;

    return true;
}
#endif



esp_err_t conv_start_successfully;

decode_frame_t decode_frame;
adc_continuous_handle_t handle;
uint8_t result[READ_LEN];


/**
 * @brief Decode task: activated by the isr callback of the 'start_decode' push button
 * 
 * @param decode_frame :Decode frame
 * @param handle : ADC handle 
 */

uint64_t prev_button_time = 0; 

void button_start_decode(){
        while (1) {
            if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)){;

            uint64_t curr_time = esp_timer_get_time(); 

            if (curr_time - prev_button_time < 5000000){
                continue;
            }

            prev_button_time = curr_time; 
            
            ESP_LOGI("DIVX_MAIN", "BUTTON_START_DECODE"); 
            // esp_err_t 


            conv_start_successfully = adc_continuous_stop(handle);

            // adc_continuous_stop(handle); 
            if (!conv_start_successfully){
                decode_frame.decode_state = TERMINATION;
                
                continue;  
                // ESP_LOGI(TAG, "Failed to stop ADC"); 
            }

            memset(result, 0x00, sizeof(result));
            x_prepare_decoder(&decode_frame);
            vTaskDelay(pdMS_TO_TICKS(1200)); 
            ESP_ERROR_CHECK_WITHOUT_ABORT(adc_continuous_start(handle));
            // if (conv_start_successfully){
            //     ESP_LOGI(TAG, "Failed to start ADC"); 
            // }
        }
        }
    }


static bool IRAM_ATTR scan_button_isr_handler (void* arg){
    BaseType_t mustYield = pdFALSE;
    //Notify that the start_decode button has been pushed
    //Todo: Handle debounce and quick presses
    vTaskNotifyGiveFromISR(button_start_task_handle, &mustYield);
    return (mustYield == pdTRUE);   
}


void app_main(void)
{

    // Probably better to do config in specialized functions so the config variables don't
    // eat up the stack. Config doesn't take up much space atm so simplicity >>>>

    esp_err_t ret;


    /**
     * @brief IO initialization -- initialize pin for scan button
     */
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SCAN_RESET_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_POSEDGE;

    gpio_config(&io_conf); 

    gpio_set_intr_type(SCAN_RESET_PIN, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SCAN_RESET_PIN, scan_button_isr_handler, NULL);


    /**
     * @brief Bluetooth Low Energy framework initialization     * 
     */

    /* Initialize NVS. */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(DIVX_BLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(DIVX_BLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(DIVX_BLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(DIVX_BLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(DIVX_BLE_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(DIVX_BLE_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(DIVX_APP_ID);
    if (ret){
        ESP_LOGE(DIVX_BLE_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(DIVX_BLE_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    // Initialize ADC call parameters
    uint8_t result[READ_LEN];
    uint32_t ret_num = 0;
    memset(result, 0xcc, READ_LEN);

    // Handle to current task
    s_task_handle = xTaskGetCurrentTaskHandle();

    // ADC Initialization 
    // adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };

    // Decode Frame Initialization 
    // decode_frame_t decode_frame; 
    x_initialize(&decode_frame);

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    // ESP_ERROR_CHECK(adc_continuous_start(handle));
    // All calls to start the ADC must come from the start-scan button. 

    // ESP_ERROR_CHECK(adc_continuous_start(handle)); 

    // ADC is started within this task and should be stopped by the main task after a decode
    // fails or is successful 
    xTaskCreate(button_start_decode, "Start Decoding", 4096, NULL, 10, &button_start_task_handle);

    while(1) {

        // This notify comes when the ADC has prepared a frame.
        if (!ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) continue;

        vTaskDelay(100/portTICK_PERIOD_MS);

        while (1) {
            ret = adc_continuous_read(handle, result, READ_LEN, &ret_num, 0);
            // int num_samples = 0; 
            if (ret == ESP_OK) {
            //     // ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32, ret, ret_num);
            //     if (decode_frame.decode_state == TERMINATION){

            //         //Stop ADC
            //         // vTaskDelay(50);
            //         ESP_LOGI("DIVX_MAIN", "Stopping continuous mode ADC");
            //         // ESP_ERROR_CHECK_WITHOUT_ABORT(adc_continuous_stop(handle));

            //         // ESP_ERROR_CHECK_WITHOUT_ABORT(adc_continuous_stop(handle)); 
            //         break; 
            //     }

                if (decode_frame.decode_state != TERMINATION)
                    x_decode(&decode_frame, &result, ret_num);

                
                
            } else{
                break; 
                if (ret == ESP_ERR_TIMEOUT) 
                    break;
            }
            //  else if (ret == ESP_ERR_TIMEOUT) {
            //     //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
            //     break;
            // }
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
