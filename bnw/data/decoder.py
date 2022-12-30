from bisect import bisect_left
import math
import os


class BinDecoder():
    """Bin-based decocder for x-codes

    I think this is the one :)

    Constructor Parameters:
        num_bins: number of bins to use, should be a power of 2 less than (2**ADC_WIDTH)
        threshold_dist: Distance from max_bin at which threshold factor should be applied
        threshold_factor: 
        
    """

    def __init__(self, num_bins, threshold_factor, threshold_dist, VALUE_WIDTH):
        self.bins = [0] * num_bins
        self.threshold_factor = threshold_factor
        self.threshold_dist = threshold_dist
        self.shift = VALUE_WIDTH - int(math.log(num_bins, 2))
        self.prev_bin = -1
        self.max_bin = 0 #bin with most values in a decoding sub-frame 


    def __clear_bins(self):
        """
        Reset decoder parameters to decode next letter
        """
        for i in range(len(self.bins)):
            self.bins[i] = 0
        self.max_bin = -1
        self.prev_bin = -1
    
    def decode(self, sequence):

        """
        Decode a sequence of ADC readings.

        Arguments:
            sequence: Iterable of integer values 
        """
        f = open("bin_decoder.log", "w") # for debugging TODO: Formalize or remove
        print("logging output to bin_decoder.log")

        for reading in sequence: 
            bin = reading >> self.shift
            # print(bin)
            self.bins[bin] += 1

            if self.bins[bin] > self.bins[self.max_bin]:
                self.max_bin = bin

            if self.max_bin == -1:
                self.max_bin = bin 

            if self.prev_bin == -1:
                self.prev_bin = bin
            else:
                if abs(bin-self.max_bin) >= self.threshold_dist:
                    direction = 1 if bin > self.max_bin else -1
                    if self.bins[bin - direction] < (self.bins[self.max_bin]//self.threshold_factor):
                        f.write(str(self.max_bin) + "\n")
                        # f.write(str(self.max_bin) + " " + str(self.bins[self.max_bin]) + "\n")
                        self.__clear_bins()  

        f.close()

class CountDecoder():
    """
    Count-based decoder for x-codes

    Parameters:
        buckets: dict -> dictionary of measured adc reading ranges for each level
        threshold: float -> minimum count (as % of first count) to be accepted as a valid code
        window_width: int -> window-width for calculating running averages
    """
    def __init__(self, buckets, threshold=0.15, window_width=16):
        self.buckets = buckets
        self.threshold = threshold
        self.window_width = window_width

        self._search_list = [val for i in buckets.keys() for val in buckets[i]]

        self._bucket_list = [i+1 for i in range(len(self._search_list)-1)]
        self._window = [0] * window_width
        self._window_sum = 0
        self._window_idx = 0
        self._window_ave = 0
        self._window_shift_div = int(math.log(window_width, 2))

        self._hold_time = -1 # minimum count to be accepted as a valid level
        self._count = 0 # running count of current bucket
        self._seen_peak = False
        self._prev_bucket = 0

    
    def _get_bucket(self, reading):
        """
        Translate reading to a "level"

        parameters:
            reading: int
        
        Returns:
            bucket: int 
            ** this is a misnomer and should be corrected **
        """

        reading = max(0, reading)
        reading = min(3100-1, reading)
        bucket = bisect_left(self._search_list, reading)
        return bucket

    def decode(self, sequence):  
        """
        Prints decoded levels from a sequence to stdout

        Parameters:
            sequence: Iterable -> iterable of adc readings
        """
        
        prev_dec = 9
        f = open("count_decoder.log", "w") # for debugging TODO: Formalize or remove
        print("logging output to 'count_decoder.log'")
        for reading in sequence:
            self._window_sum += reading - self._window[self._window_idx]
            self._window_ave = self._window_sum >> self._window_shift_div
            self._window[self._window_idx] = reading

            bucket = self._get_bucket(self._window_ave)
            f.write(str(bucket) + "\n")

            if self._hold_time == -1:
                if bucket == self._bucket_list[-2] and self._seen_peak:
                    self._hold_time = int(self.threshold * self._count)
                    # print(self._hold_time,"\n")
                    self._count = 0
                if bucket == self._bucket_list[-1]:
                    self._seen_peak = True
                    self._count += 1

            else:
                # bucket = self._get_bucket(self._window_ave)
                if bucket == self._prev_bucket:
                    self._count += 1
                    if (self._count >= self._hold_time) and (bucket == 1):
                        # print("\n")
                        self.reset()
                else:
                    # print(self._count)
                    
                    if self._count >= self._hold_time  and self._prev_bucket%2:
                        if self._prev_bucket != prev_dec: 
                            # print(self._prev_bucket, self._count, self._hold_time)
                            prev_dec = self._prev_bucket
                    self._count = 1

                        
            self._prev_bucket = bucket

            if self._window_idx < (self.window_width-1):
                self._window_idx += 1
            else: 
                self._window_idx = 0
        
        f.close()

    
    def reset(self):
        """
        Reset class parameters in preparation for decoding a code word

        """

        self._window = [0] * self.window_width
        self._window_sum = 0
        self._window_idx = 0
        self._window_ave = 0
        self._hold_time = -1
        self._count = 0
        self._seen_peak = False
        self._prev_bucket = 0


        
if __name__ == "__main__":


    buckets = {
    0: (0, 64),
    1: (140, 200),
    2: (1500, 1750),
    3: (2400, 2650),
    4: (2850, 3100)
}
    
    script_dir = os.path.dirname(__file__)
    rel_path = "sample_reads/sample_reads3.txt"
    abs_file_path = os.path.join(script_dir, rel_path)

    with open(abs_file_path, "r") as f:
        samples = f.readlines()
        samples = [int(x) for x in samples]


    ## Run count decoder. Logs output to count_decoder.log
    count_decoder = CountDecoder(buckets)
    count_decoder.decode(samples)

    ## Run Bin Decoder. Logs output to bin_decoder.log
    bin_decoder = BinDecoder(num_bins=512, threshold_factor=2, threshold_dist=4, VALUE_WIDTH=12)
    bin_decoder.decode(samples)                 
