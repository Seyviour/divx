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


//DMA BUFFER SIZE 
#define READ_LEN   256

#define GET_UNIT(x)        ((x>>3) & 0x1)

//ADC RELATED
#if CONFIG_IDF_TARGET_ESP32
#define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1  //ESP32 only supports ADC1 DMA mode
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE1
static adc_channel_t channel[1] = {ADC_CHANNEL_6};
#endif

//

#define NUMBER_DECODE_BINS 256
#define BIN_HISTORY_SIZE 4
#define MOVING_AVERAGE_WINDOW_WIDTH 32

static TaskHandle_t s_task_handle;
static const char *TAG = "EXAMPLE";


enum decode_state{
    BUSY,
    IDLE 
} ; 

typedef struct {
    bool peak_seen; 
    enum decode_state state;
    uint16_t decode_bins[NUMBER_DECODE_BINS];
    uint16_t prev_bin; 
    uint16_t max_bin;
    uint16_t threshold_factor;
    uint16_t threshold_dist;
    uint16_t prev_log; 
}decode_frame_t; 

void x_reset(decode_frame_t *decode_frame){
    decode_frame->peak_seen = false; 
    memset(decode_frame->decode_bins, 0x00, 512);
    decode_frame->prev_bin = 0;
    decode_frame->max_bin = 0;
}


void x_decode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num){
    // x_df -> x_code decode frame 
    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
        adc_digi_output_data_t *p = (void*)&adc_stream[i];
        uint16_t this_reading = p->type1.data;
        uint8_t bin = this_reading >> 4; 
        x_df->decode_bins[bin] += 1;

        
        if (x_df->decode_bins[bin] > x_df->decode_bins[x_df->max_bin]){
            x_df->max_bin = bin;
            x_df->prev_bin = bin;
            return; 
        }

        if (bin == x_df->prev_bin ) return;

        // If I'm here then the current bin is not the max_bin

        uint8_t direction = (bin > x_df->max_bin)? 1: 0; 
        uint16_t dist = (direction == 1)? (bin - x_df->max_bin): (x_df->max_bin-bin);


        if ((x_df->decode_bins[x_df->max_bin] > 500) && (dist >= 3)){
            uint16_t check_bin = (direction == 1)? (bin - 1): (bin+1);
            if (x_df->decode_bins[check_bin] <= (x_df->decode_bins[x_df->max_bin] << 2)){
                if (x_df->max_bin != x_df->prev_log){
                    ESP_LOGI(TAG, "%" PRIu16 "\n", x_df->max_bin);
                    x_df->prev_log = x_df->max_bin;
                    x_reset(x_df); 
                }
            }    
        }   
    }

}


static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
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

void app_main(void)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[READ_LEN] = {0};
    memset(result, 0xcc, READ_LEN);

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));



    decode_frame_t decode_frame = {
        .state = IDLE,
        .threshold_factor = 0,
        .threshold_dist = 0,
        .prev_log = 0
    };
    x_reset(&decode_frame); 

    while(1) {

        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1) {
            ret = adc_continuous_read(handle, result, READ_LEN, &ret_num, 0);
            // int num_samples = 0; 
            if (ret == ESP_OK) {
                // ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32, ret, ret_num);
                x_decode(&decode_frame, &result, ret_num); 
                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                // vTaskDelay(100);
            } else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
