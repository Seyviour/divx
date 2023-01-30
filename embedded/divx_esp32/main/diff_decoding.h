
#define CAPTURE_WINDOW_SIZE 8
#define BIN_WINDOW_SIZE 8
#define NUMBER_DECODE_BINS 256


typedef struct {
    uint8_t start;
    uint8_t centre;
    uint8_t end;
    bool valid; 
} detect_pattern_t; 

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

    detect_pattern_t old_pattern; 
    detect_pattern_t detect_patterns[2]; 

    /* data */
} decode_frame_t;
void incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num);
uint8_t pickBins(decode_frame_t *x_df);
// void incrementalClear(decode_frame_t *x_df);
void x_reset(decode_frame_t*, uint8_t, uint8_t, uint8_t);
void x_resolve_reset(decode_frame_t *x_df); 


void incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num){
    ESP_LOGI("DEBUG", "CALLING INCREMENTAL DECODE");
    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
        adc_digi_output_data_t *p = (void*)&adc_stream[i];
        uint16_t reading = p->type1.data;
        x_df->capture_window_sum += -x_df->capture_window[x_df->capture_window_idx] + reading;
        x_df->capture_window[x_df->capture_window_idx] = reading;
        x_df->capture_window_idx = (x_df->capture_window_idx>=CAPTURE_WINDOW_SIZE-1)?
                                        0 : x_df->capture_window_idx+1;
        uint16_t bin = (x_df->capture_window_sum >> 3) >> 4;

        //NAIVE OVERFLOW HANDLING
        if (x_df->decode_bins[bin]<UINT16_MAX)
            x_df->decode_bins[bin] += 1;

        x_df->frame_count += 1;

        if(x_df->frame_count >= x_df->cadence){
            x_df->frame_count = 0;
            uint8_t num_bins_picked = pickBins(x_df);
            x_resolve_reset(x_df);

        }
       
    }
}



uint8_t pickBins(decode_frame_t *x_df){

    //DUMPING THESE ON THE STACK. I THINK IT'S APPROPRIATE. MFJPM

    uint16_t smooth_bins[256]; 
    x_df->bin_window_sum = 0;
    x_df->bin_window_idx = 0;
    memset(x_df->bin_window, 0, sizeof(x_df->bin_window[1]) * BIN_WINDOW_SIZE); 

    ESP_LOGI("DEBUG", "CALLING PICKBINS"); 

    ESP_LOGI("DEBUG", "DECODE BINS"); 
    for (int j = 0; j<256; j++){
        printf("%"PRIu16" ", x_df->decode_bins[j]); 
    }

    // uint16_t pattern[NUMBER_DECODE_BINS] = {0};
    memset(x_df->pattern, 0, NUMBER_DECODE_BINS);
    uint16_t prev = 0;
    int8_t state = 0;
    int8_t curr = 0;

    ESP_LOGI("DEBUG", "PICKBINS STATE 0");

    for (int i = 0; i < NUMBER_DECODE_BINS; i++){
        x_df->bin_window_sum += x_df->decode_bins[i] - x_df->bin_window[x_df->bin_window_idx];
        x_df->bin_window[x_df->bin_window_idx] = x_df->decode_bins[i];
        x_df->bin_window_idx = (x_df->bin_window_idx>=BIN_WINDOW_SIZE-1)?
                                        0 : x_df->bin_window_idx+1;
        
        smooth_bins[i] = x_df->bin_window_sum>>3; //This needs to be parameterized

        prev = (i <= 0)? 0: smooth_bins[i-1]; 

        if (smooth_bins[i] > prev)
            x_df->pattern[i] = 1;
        else if (smooth_bins[i] < prev)
            x_df->pattern[i] = -1;
        else
            x_df->pattern[i] = 0; 
        
        uint16_t backA = 0; 
        uint16_t backB = 0;
        int8_t diff = 0; 

        if (i >= BIN_WINDOW_SIZE) backB = smooth_bins[i-(BIN_WINDOW_SIZE-1)];
        if (i >= BIN_WINDOW_SIZE+1) backA = smooth_bins[i-(BIN_WINDOW_SIZE)];
        
        if (backB > backA) diff = 1;
        else if (backA > backB) diff = -1;

        x_df->pattern[i] = -diff + x_df->pattern[i];
        if (i>0) x_df->pattern[i] += x_df->pattern[i-1];


        // STATE MACHINE
        if (state == 0){
            
            if (abs(x_df->pattern[i]) <= 1){
                if (curr >= 2) return curr;
                //I DON'T LIKE THIS. POTENTIALLY TOO MANY MEMORY ACCESSES, ESPECIALLY ON SMALL PROCESSORS
                x_df->detect_patterns[curr].start = i;
            }

            if (x_df->pattern[i] >= 3){
                state = 1;
                ESP_LOGI("DEBUG", "PICKBINS STATE 1");
            }
                continue;

        }

        else if (state == 1){
            

            if (abs(x_df->pattern[i])<=1){
                x_df->detect_patterns[curr].centre = i; 
                state =2;
                ESP_LOGI("DEBUG", "PICKBINS STATE 2");
                // ESP_LOGI("DETECTION", "BIN: %d", i); 
            }
        }

        else if (state == 2){
            
            if (x_df->pattern[i] <= -3){
                state = 3;
                ESP_LOGI("DEBUG", "PICKBINS STATE 3");
            }
        }

        else if (state == 3){
            

            if (abs(x_df->pattern[i]) <= 1){
                x_df->detect_patterns[curr].end = i; 
                x_df->detect_patterns[curr].valid = true; 
                state = 0;
                curr += 1;
            }
        }

    }
    ESP_LOGI("DEBUG", "SMOOTH BINS");
    for (int j = 0; j<256; j++){
        printf("%"PRIu16" ", smooth_bins[j]); 
    }

    ESP_LOGI("DEBUG", "PATTERN LOG"); 
    for (int j = 0; j<256; j++){
        printf("%"PRIi8" ", x_df->pattern[j]); 
    }

    return curr; 

}

uint8_t abs_diff(uint8_t a, uint8_t b){
    return (a > b? (a-b): (b-a)); 
}

void x_resolve_reset(decode_frame_t *x_df){
    ESP_LOGI("INFO", "CALLING x_resolve_reset");

    if (! x_df->old_pattern.valid){
        if (x_df->detect_patterns[0].valid){
            x_df->old_pattern = x_df->detect_patterns[0];
        }
    }

    if (x_df->detect_patterns[0].valid && x_df->detect_patterns[1].valid){

        uint8_t diff0 = abs_diff(x_df->detect_patterns[0].centre, x_df->old_pattern.centre);
        uint8_t diff1 = abs_diff(x_df->detect_patterns[1].centre, x_df->old_pattern.centre);

        uint8_t emit, retain; 
        if(diff0 < diff1){
            emit = 0;
            retain = 1; 
        } else {
            emit = 1;
            retain = 0;
        }

        ESP_LOGI("DETECTION EVENT", "%"PRIu8" %"PRIu8" %"PRIu8, x_df->detect_patterns[emit].start, x_df->detect_patterns[emit].centre, x_df->detect_patterns[emit].end); 
        x_reset(x_df, 1, x_df->detect_patterns[retain].start, x_df->detect_patterns[retain].end);
                    
    }
}



void x_initialize(decode_frame_t *x_df){

    // THESE SHOULD BE ARGUMENTS
    x_df->bin_width = 12;
    x_df->shift_factor = 3;
    x_df->cadence = 256;
    x_df->window_width = 8;
    x_df->threshold = 3;
    x_df->old_pattern.valid = false;
    x_reset(x_df, 1, 255, 255); 
}

void x_reset(decode_frame_t *x_df, uint8_t reset_type, uint8_t preserve_start, uint8_t preserve_end){

    //reset type: 0 - decode_bins is not cleared
    //reset type: 1 - decode_bins is cleared


    // ttoi, I could have used memset
    if(reset_type==1){
        for (uint8_t i = 0; (i<preserve_start && i<NUMBER_DECODE_BINS-1);  i++){
            x_df->decode_bins[i] = 0;
        }
        for (uint8_t i = preserve_end; i < NUMBER_DECODE_BINS-1; i++){
            x_df->decode_bins[i] = 0; 
        }
    }

    x_df->detect_patterns[0].valid = false;
    x_df->detect_patterns[1].valid = false; 

    memset(x_df->capture_window, 0x00, sizeof(x_df->capture_window[1])*CAPTURE_WINDOW_SIZE); 
    x_df->capture_window_sum = 0;
    x_df->capture_window_idx = 0;

    x_df->frame_count = 0;

    memset(x_df->bin_window, 0x00, sizeof(x_df->bin_window[0])*BIN_WINDOW_SIZE);
    x_df->bin_window_idx = 0;
    x_df->bin_window_sum = 0;

    memset(x_df->pattern, 0x00, sizeof(x_df->pattern[0])*NUMBER_DECODE_BINS); 
}