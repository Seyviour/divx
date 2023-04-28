

/*
/ X-Code Decoding Based on "Significance" Levels
/ Expected average for each bin after number_of_bins readings is 1
/ 16 is significant
*/

#define x_HISTORY_SIZE 16
#define x_CADENCE 256
#define x_SHIFT_FACTOR 3 
#define CAPTURE_WINDOW_SIZE 128
#define BIN_WINDOW_SIZE 8
#define NUMBER_DECODE_BINS 256
#define x_df_THRESHOLD 2
#define x_SIG_THRESHOLD 300
#define x_type uint8_t
#define PRE_IGNORE 1024

#define DECLARATION_WIDTH 4


uint8_t abs_diff(uint8_t a, uint8_t b){
    return (a > b? (a-b): (b-a)); 
}

unsigned char LETTERS[4] = {'A', 'B', 'C', 'D'}; 

enum decode_state {
    IDLE,
    DECLARATION,
    MESSAGE, 
    TERMINATION,
    ERROR 
}; 

typedef enum decode_state decode_state_t; 

typedef struct {
    uint8_t start;
    uint8_t max;
    uint8_t end;
    bool valid; 
} detect_pattern_t;

typedef struct {
    uint32_t num_samples; 
    enum decode_state decode_state; 
    unsigned char decode_history[x_HISTORY_SIZE]; 
    uint8_t emit_history[x_HISTORY_SIZE];
    uint32_t emit_time[x_HISTORY_SIZE]; 
    uint32_t emit_strength[x_HISTORY_SIZE]; 
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

    uint8_t idx_offset;

    uint16_t pre_ignore; 

    detect_pattern_t old_pattern; 
    detect_pattern_t detect_patterns[2]; 

    /* data */
} decode_frame_t;

decode_state_t incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num);
uint8_t pickBins(decode_frame_t *x_df);
// void incrementalClear(decode_frame_t *x_df);
void x_reset(decode_frame_t*, uint8_t, uint8_t, uint8_t);
void x_resolve_reset(decode_frame_t *x_df);
void x_determine(decode_frame_t *x_df);
void x_prepare_decoder(decode_frame_t *x_df);


decode_state_t incrementalDecode(decode_frame_t *x_df, uint8_t *adc_stream, uint32_t ret_num){

    if (x_df->decode_state == TERMINATION) {
        ESP_LOGI("DECODER", "Invalid state to decode");
        return x_df->decode_state; 
    }

    if (x_df->pre_ignore < PRE_IGNORE){
        x_df->pre_ignore += ret_num/SOC_ADC_DIGI_RESULT_BYTES; 
        return x_df->decode_state; 
    }
    // ESP_LOGI("DEBUG", "CALLING INCREMENTAL DECODE");
    x_df->num_samples += ret_num/SOC_ADC_DIGI_RESULT_BYTES; 
    // printf("%"PRIu32" SAMPLES DECODED \n", x_df->num_samples);
     
    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES*2) {
        adc_digi_output_data_t *p = (void*)&adc_stream[i];
        uint16_t reading = p->type1.data;
        x_df->capture_window_sum += -x_df->capture_window[x_df->capture_window_idx] + reading;
        x_df->capture_window[x_df->capture_window_idx] = reading;
        x_df->capture_window_idx = (x_df->capture_window_idx>=CAPTURE_WINDOW_SIZE-1)?
                                        0 : x_df->capture_window_idx+1;
        // uint16_t bin = (x_df->capture_window_sum >> 7) >> 4;
        uint16_t bin = reading >> 4; 

        //NAIVE OVERFLOW HANDLING
        if (x_df->decode_bins[bin]<1024)
            x_df->decode_bins[bin] += 1;

        x_df->frame_count += 1;

        if(x_df->frame_count >= x_CADENCE){
            x_df->frame_count = 0;
            uint8_t num_bins_picked = pickBins(x_df);
            x_resolve_reset(x_df);

        }
       
    }
    return x_df->decode_state; 
}

uint8_t pickBins(decode_frame_t *x_df){

    // ESP_LOGI("DEBUG", "DECODE BINS"); 
    for (int j = 0; j<256; j++){
        if (x_df->decode_bins[j]>=50)
        printf("%03d:%04"PRIu16"   ", j, x_df->decode_bins[j]); 
    }
    printf("\n\n"); 

    // NAIVELY COMPUTE AVERAGE
    // A BETTER IMPLEMENTATION WOULD TAKE ADVANTAGE OF INFORMATION FROM ```incrementalDecode```

    uint32_t bin_ave = 0;
    for (int j=0; j<256; j++){
        bin_ave += x_df->decode_bins[j]; 
    }

    bin_ave = bin_ave >> 8; 
    // printf("BIN AVERAGE %"PRIu32, bin_ave);
    // bin_ave = bin_ave >> 8; 
     
    // return 0; 

    x_df->detect_patterns[0].valid = false;
    x_df->detect_patterns[1].valid = false; 
    // return 0; 
    
    bool detect = false; 
    uint8_t sig_seen = 0; 
    uint16_t bin5sum = x_df->decode_bins[0] + x_df->decode_bins[1];
    uint16_t max3sum = 0;
    uint16_t sub = 0;
    // uint16_t SOME_THRESHOLD = bin_ave +  (bin_ave << 4);
    uint16_t SOME_THRESHOLD = 225; 
    printf("Threshold %"PRIu16"\n", SOME_THRESHOLD); 

    for (int i = 1; i < NUMBER_DECODE_BINS-2; i++){
        // bin5sum += x_df->decode_bins[i+1] - sub;
        bin5sum = x_df->decode_bins[i]; 
        bin5sum += ((i > 0)?x_df->decode_bins[i-1]:0); 
        bin5sum += ((i > 1)?x_df->decode_bins[i-2]:0); 
        bin5sum += ((i < 255)?x_df->decode_bins[i+1]:0);
        bin5sum += ((i < 254)? x_df->decode_bins[i+2]:0); 

        // if (detect && bin5sum<=x_df->detect_patterns[sig_seen].max >> 2){
        if (detect && bin5sum<=120){
            if (sig_seen == 0) x_df->emit_strength[x_df->num_emits] = max3sum;
            x_df->detect_patterns[sig_seen].end = i-1;
            sig_seen += 1;
            detect = false;
        }

        else if (bin5sum >= SOME_THRESHOLD && !detect){
            detect = true;
            x_df->detect_patterns[sig_seen].max = i;
            x_df->detect_patterns[sig_seen].valid = true;
            x_df->detect_patterns[sig_seen].start = i-1;
            max3sum = bin5sum;
            
        }

        else if (detect && bin5sum>max3sum){
            max3sum = bin5sum; 
            x_df->detect_patterns[sig_seen].max = i;
            
        }

        sub = x_df->decode_bins[i-1]; 
    
    }

    return sig_seen; 
}


void x_resolve_reset(decode_frame_t *x_df){
    // ESP_LOGI("INFO", "CALLING x_resolve_reset");

    // printf("1"); 
    if (! x_df->old_pattern.valid){
        if (x_df->detect_patterns[0].valid){
            x_df->old_pattern = x_df->detect_patterns[0];
        }
    }

    // printf("2"); 
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

        // printf("3"); 
        // ESP_LOGI("DETECTION EVENT", "OLD PATTERN ""%"PRIu8" %"PRIu8" %"PRIu8, x_df->old_pattern.start, x_df->old_pattern.max, x_df->old_pattern.end); 
        // printf("4"); 
        // ESP_LOGI("DETECTION EVENT", "EMIT %"PRIu8" %"PRIu8" %"PRIu8, x_df->detect_patterns[emit].start, x_df->detect_patterns[emit].max, x_df->detect_patterns[emit].end); 
        // ESP_LOGI("DETECTION EVENT", "PRESERVE ""%"PRIu8" %"PRIu8" %"PRIu8, x_df->detect_patterns[retain].start, x_df->detect_patterns[retain].max, x_df->detect_patterns[retain].end);
        // printf("5"); 
        x_df->old_pattern = x_df->detect_patterns[retain];
        x_df->emit_history[x_df->num_emits] = x_df->detect_patterns[emit].max;
        x_df->emit_time[x_df->num_emits] = x_df->num_samples; 
        x_df->num_emits += 1; 
        x_determine(x_df);
        
        x_reset(x_df, 1, x_df->detect_patterns[retain].start, x_df->detect_patterns[retain].end);
                       
    }
}

void x_determine(decode_frame_t *x_df){
    // ESP_LOGI("SIG_DECODER", "Calling x_determine"); 
    if (x_df -> num_emits >= 32) {
		ESP_LOGI("DEBUG", "Exceeded decode buffer size");
		x_df->decode_state = TERMINATION;
	}
	
	if (x_df->num_emits == 1){
		x_df->decode_history[0] = 'A';
		return; 
	}
	
	const uint8_t current_emit = x_df->emit_history[x_df->num_emits-1];
	const uint8_t prev_emit = x_df->emit_history[x_df->num_emits-2];  
	
	if (x_df->decode_state == DECLARATION){
		if (x_df->num_emits > DECLARATION_WIDTH){
			if(current_emit < prev_emit){
				x_df->decode_state = MESSAGE;
			} else {
                x_df -> idx_offset += 1; 
            }
		
		} else{
			if (current_emit < prev_emit){
				ESP_LOGI("DECODER->x_determine", "Decode Error: Invalid declatation region");
				x_df->decode_state = ERROR;
			} else{
				x_df->decode_history[x_df->num_emits-1] = LETTERS[x_df->num_emits-1];  
			}
				
		}
	}
	
	if (x_df->decode_state == MESSAGE){
		uint16_t min_dist = UINT16_MAX;
		uint16_t this_dist = UINT16_MAX;
        unsigned char curr_letter = '.';
        unsigned char prev_letter = x_df->decode_history[x_df->num_emits-1-x_df->idx_offset-1]; 
		int i = 0;
		for (i = 0; i < DECLARATION_WIDTH; i++){
			this_dist = abs_diff(current_emit, x_df->emit_history[i]); 
			if (this_dist < min_dist){
				curr_letter=LETTERS[i];
				min_dist = this_dist; 
			}		
		}
        if (curr_letter != prev_letter) {
            x_df->decode_history[x_df->num_emits-1-x_df->idx_offset]=curr_letter; 
        } else {
            x_df->idx_offset+=1; 
        }

		
		if (x_df->decode_history[x_df->num_emits-1-x_df->idx_offset]=='A' && x_df->decode_history[x_df->num_emits-2-x_df->idx_offset] == 'D'){
			x_df->decode_state = TERMINATION; 
		}  
		
	}
	
	if (x_df->decode_state == TERMINATION) {
		ESP_LOGI("DECODER->x_determine", "Decoding done or in improper state to decode"); 
	}

    ESP_LOGI("DEBUG", "DEECODE_HISTORY"); 
    for (int j = 0; j<x_HISTORY_SIZE; j++){
        printf("%c ", x_df->decode_history[j]); 
    }
    printf("\n"); 

    ESP_LOGI("DEBUG", "EMIT TIME"); 
    for (int j = 0; j<x_HISTORY_SIZE; j++){
        printf("%"PRIu32" ", x_df->emit_time[j]); 
    }
    printf("\n"); 
    
    ESP_LOGI("DEBUG", "EMIT HISTORY"); 
    for (int j = 0; j<x_HISTORY_SIZE; j++){
        printf("%03"PRIu8" ", x_df->emit_history[j]); 
    }
    printf("\n"); 

    ESP_LOGI("DEBUG", "EMIT STRENGTH");
    for (int j = 0; j<x_HISTORY_SIZE; j++){
        printf("%04"PRIu32" ", x_df->emit_strength[j]); 
    }
    printf("\n"); 
    

    if (set_notify_xcode_value(x_df->decode_history)){
        ESP_LOGI("DECODER", "Failed to set xcode value"); 
    } else {
        ESP_LOGI("DECODER", "Xcode value sent"); 
    }
}

void x_initialize(decode_frame_t *x_df){

    // THESE SHOULD BE ARGUMENTS
    // memset(x_df->capture_window, 0x00, sizeof(x_df->capture_window[1])*CAPTURE_WINDOW_SIZE);
    x_df->bin_width = 12;
    x_df->shift_factor = 3;
    x_df->cadence = 256;
    x_df->window_width = 8;
    x_df->threshold = 3;
    x_prepare_decoder(x_df); 
}

void x_prepare_decoder(decode_frame_t *x_df){
    for (int j = 0; j < x_HISTORY_SIZE; j++){
        x_df->decode_history[j] = '.';
    }
    x_df->decode_state = DECLARATION; 
    memset(x_df->capture_window, 0x00, sizeof(x_df->capture_window[1])*CAPTURE_WINDOW_SIZE);
    x_df->num_samples = 0; 
    x_df->capture_window_sum = 0;
    x_df->capture_window_idx = 0;
    x_df->num_emits = 0;
    memset(x_df->emit_history, 0, sizeof(x_df->emit_history));
    memset(x_df->emit_time, 0, sizeof(x_df->emit_time));
    memset(x_df->decode_bins, 0, sizeof(x_df->decode_bins)); 
    x_df->old_pattern.valid = false;
    x_reset(x_df, 1, 255, 254);
    x_df->idx_offset = 0;
    x_df->pre_ignore = 0; 
}

void x_reset(decode_frame_t *x_df, uint8_t reset_type, uint8_t preserve_start, uint8_t preserve_end){


    /**
     * @brief Resets occur after 2 spikes have been detected in the bin array
     * When that occurs, the area around the newer bin is preserved while the 
     * rest of the bin array is set to 0
     * 
     */

    //reset type: 0 - decode_bins is not cleared
    //reset type: 1 - decode_bins is cleared


    // ttoi, I could have used memset

    ESP_LOGI("SIG_DECODING", "CALLING RESET"); 
    if(reset_type==1){
        for (uint16_t i = 0; (i<preserve_start);  i++){
            x_df->decode_bins[i] = 0;
        }
        for (uint16_t i = preserve_end+1; i < NUMBER_DECODE_BINS; i++){
            x_df->decode_bins[i] = 0; 
        }
    }

    if (reset_type ==0){
        x_df->decode_state = DECLARATION; 
    }

    x_df->detect_patterns[0].valid = false;
    x_df->detect_patterns[1].valid = false; 

    

    x_df->frame_count = 0;

    memset(x_df->bin_window, 0x00, sizeof(x_df->bin_window[0])*BIN_WINDOW_SIZE);
    x_df->bin_window_idx = 0;
    x_df->bin_window_sum = 0;

}
