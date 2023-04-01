
#define CAPTURE_WINDOW_SIZE 8
#define BIN_WINDOW_SIZE 8
#define NUMBER_DECODE_BINS 256

void incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num);
void pickBins(decode_frame_t *x_df);
typedef struct {

    uint16_t bin_width;
    uint16_t shift_factor;
    uint16_t cadence;
    uint16_t window_width;
    uint16_t threshold;

    uint16_t capture_window[CAPTURE_WINDOW_SIZE];
    uint32_t capture_window_sum;
    uint16_t capture_window_idx; 

    uint16_t decode_bins[NUMBER_DECODE_BINS]; 
    uint16_t frame_count;

    uint16_t bin_window[BIN_WINDOW_SIZE];
    uint16_t bin_window_idx;
    uint32_t bin_window_sum;
    int8_t pattern[NUMBER_DECODE_BINS];
    
    /* data */
} decode_frame_t;


void incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num){
    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
        adc_digi_output_data_t *p = (void*)&adc_stream[i];
        uint16_t reading = p->type1.data;
        x_df->capture_window_sum += -x_df->capture_window[x_df->capture_window_idx] + reading;
        x_df->capture_window[x_df->capture_window_idx] = reading;
        x_df->capture_window_idx = (x_df->capture_window_idx>=CAPTURE_WINDOW_SIZE-1)?
                                        0 : x_df->capture_window_idx+1;
        uint16_t bin = (x_df->capture_window_sum >> 5) >> 4;
        x_df->decode_bins[bin] += 1;

        x_df->frame_count += 1;

        if(x_df->frame_count >= x_df->cadence){
            x_df->frame_count = 0;
            pickBins(x_df); 
        }
    }
}

void pickBins(decode_frame_t *x_df){

    // uint16_t pattern[NUMBER_DECODE_BINS] = {0};
    memset(x_df->pattern, 0, NUMBER_DECODE_BINS);
    uint16_t prev = 0;
    int8_t state = 0;
    int8_t curr = 0;

    for (int i = 0; i < NUMBER_DECODE_BINS; i++){
        x_df->bin_window_sum += x_df->decode_bins[i] - x_df->bin_window_sum[x_df->bin_window_idx];
        x_df->bin_window[x_df->bin_window_idx] = x_df->decode_bins[i];
        x_df->bin_window_idx = (x_df->bin_window_idx>=BIN_WINDOW_SIZE-1)?
                                        0 : x_df->bin_window_idx+1;
        
        x_df->decode_bins[i] = x_df->bin_window_sum>>3; //This needs to be parameterized

        if x_df->decode_bins[i] > prev:
            x_df->pattern[i] = 1;
        else if (x_df->decode_bins[i] < prev):
            x_df->pattern[i] = -1; 
        
        uint16_t backA = 0; 
        uint16_t backB = 0;
        int8_t diff = 0; 

        if (i >= BIN_WINDOW_SIZE) backB = x_df->bins[i-BIN_WINDOW_SIZE];
        if (i >= BIN_WINDOW_SIZE+1) backA = x_df->bins[i-(BIN_WINDOW_SIZE+1)];
        
        if (backB > backA) diff = 1;
        else if (backA > backB) diff = -1;
        else diff = 0;

        x_df->pattern[i] = -diff + pattern[i];
        if (i>0) x_df->pattern[i] += x_df->pattern[i-1];


        // STATE MACHINE
        if (state == 0){
            if (abs(pattern[i]) <= 1){
                if (curr >= 2) break; 
            }

            if (x_df->pattern[i] >= 3){
                state = 1;
            }

        }

        if (state == 1){
            if (abs(x_df->pattern[i])<=1){
                state =2;
                ESP_LOGI("DETECTION", "BIN: %d", i); 
            }
        }

        if (state == 2){
            if (x_df->pattern[i] <= -3){
                state = 3;
            }
        }

        if (state == 3){
            if (abs(pattern[i]) <= 1){
                state = 0
                curr += 1;
            }
        }

    }

}







