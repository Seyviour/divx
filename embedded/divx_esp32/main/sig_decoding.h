

/*
/ X-Code Decoding Based on "Significance" Levels
/ Expected average for each bin after number_of_bins readings is 1
/ 16 is significant
*/

#define x_CADENCE 256
#define x_SHIFT_FACTOR 3 
#define CAPTURE_WINDOW_SIZE 8
#define BIN_WINDOW_SIZE 8
#define NUMBER_DECODE_BINS 256
#define x_df_THRESHOLD 2
#define x_SIG_THRESHOLD 32



enum decode_state {
    IDLE,
    DECLARATION,
    MESSAGE, 
    TERMINATION,   
} ; 

typedef struct {
    uint8_t start;
    uint8_t max;
    uint8_t end;
    bool valid; 
} detect_pattern_t;

typedef struct {

    enum decode_state decode_state; 
    char decode_history[32]; 
    uint8_t emit_history[32]; 
    uint8_t num_emits; 
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

    detect_pattern_t old_pattern; 
    detect_pattern_t detect_patterns[2]; 

    /* data */
} decode_frame_t;

void incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num);
uint8_t pickBins(decode_frame_t *x_df);
// void incrementalClear(decode_frame_t *x_df);
void x_reset(decode_frame_t*, uint8_t, uint8_t, uint8_t);
void x_resolve_reset(decode_frame_t *x_df);
void x_determine(decode_frame_t *x_df, uint8_t resolve_idx);


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

    ESP_LOGI("DEBUG", "DECODE BINS"); 
    for (int j = 0; j<256; j++){
        printf("%04"PRIu16" ", x_df->decode_bins[j]); 
    }

    uint8_t sig_seen = 0; 
    for (int i = 0; i < NUMBER_DECODE_BINS; i++){
        if (x_df -> decode_bins[i] >= x_SIG_THRESHOLD) {
            x_df -> detect_patterns[sig_seen].start = i;
            x_df -> detect_patterns[sig_seen].max = i;
            x_df -> detect_patterns[sig_seen].valid = true; 
            i++;
            while (x_df->decode_bins[i] >= 16) {
                i++;
                if (x_df->decode_bins[i]>x_df->decode_bins[x_df->detect_patterns[sig_seen].max])
                    x_df->detect_patterns[sig_seen].max = i;
            };
            --i;
            x_df->detect_patterns[sig_seen].end = i; 
            sig_seen += 1; 
        }
    }

    return sig_seen; 
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

        uint8_t diff0 = abs_diff(x_df->detect_patterns[0].max, x_df->old_pattern.max);
        uint8_t diff1 = abs_diff(x_df->detect_patterns[1].max, x_df->old_pattern.max);

        uint8_t emit, retain; 
        if(diff0 < diff1){
            emit = 0;
            retain = 1; 
        } else {
            emit = 1;
            retain = 0;
        }

       ESP_LOGI("DETECTION EVENT", "OLD PATTERN ""%"PRIu8" %"PRIu8" %"PRIu8, x_df->old_pattern.start, x_df->old_pattern.max, x_df->old_pattern.end); 

        ESP_LOGI("DETECTION EVENT", "EMIT %"PRIu8" %"PRIu8" %"PRIu8, x_df->detect_patterns[emit].start, x_df->detect_patterns[emit].max, x_df->detect_patterns[emit].end); 
        ESP_LOGI("DETECTION EVENT", "PRESERVE ""%"PRIu8" %"PRIu8" %"PRIu8, x_df->detect_patterns[retain].start, x_df->detect_patterns[retain].max, x_df->detect_patterns[retain].end); 
        x_df->old_pattern = x_df->detect_patterns[retain];
        x_determine(x_df, emit);
        x_reset(x_df, 1, x_df->detect_patterns[retain].start, x_df->detect_patterns[retain].end);
                       
    }
}

void x_determine(decode_frame_t *x_df, uint8_t resolve_idx){

    if (x_df->num_emits >= 32){
        ESP_LOGI("DEBUG", "Exceeded buffer size for decoding"); 
    }

    const detect_pattern_t current_pattern = x_df->detect_patterns[resolve_idx];

    if (x_df->decode_state != MESSAGE && x_df->num_emits>=3 && current_pattern.max < x_df->emit_history[x_df->num_emits-1]){
        x_df->decode_state = MESSAGE; 
    }

    uint16_t min_dist = UINT16_MAX;
    uint16_t curr_dist; 
    unsigned char letter = '.';

    

    if (x_df->decode_state == MESSAGE){
        for (uint8_t i = 0; i < x_df->num_emits; i++){
            if ((curr_dist = abs_diff(current_pattern.max, x_df->emit_history[i])) < min_dist){
                letter = x_df->decode_history[i];
                min_dist = curr_dist; 
            }
        }
    } else if (x_df->decode_state == DECLARATION) {
        letter = (unsigned char) ('A' + x_df->num_emits);
    }

    x_df->decode_history[x_df->num_emits] = letter;
    
    x_df->emit_history[x_df->num_emits]=x_df->detect_patterns[resolve_idx].max;
    x_df->num_emits += 1;
    
    ESP_LOGI("DEBUG", "EMIT HISTORY");

    for (uint8_t i = 0; i<12; i++){
        printf("%04"PRIu8" ", x_df->emit_history[i]);
        printf("%c", x_df->decode_history[i]);  
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
    x_df->num_emits = 0;
    memset(x_df->emit_history, 0, sizeof(x_df->emit_history));
    x_reset(x_df, 1, 255, 255); 
}

void x_reset(decode_frame_t *x_df, uint8_t reset_type, uint8_t preserve_start, uint8_t preserve_end){

    //reset type: 0 - decode_bins is not cleared
    //reset type: 1 - decode_bins is cleared


    // ttoi, I could have used memset
    if(reset_type==1){
        for (uint16_t i = 0; (i<preserve_start);  i++){
            x_df->decode_bins[i] = 0;
        }
        for (uint16_t i = preserve_end+1; i < NUMBER_DECODE_BINS; i++){
            x_df->decode_bins[i] = 0; 
        }
    }

    if (reset_type ==0){
        x_df->decode_state = IDLE; 
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

}
