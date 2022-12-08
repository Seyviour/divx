from bisect import bisect_left
import math


class CountDecoder():
    """
    Count-based decoder for x-codes

    Parameters:
        buckets: dict -> dictionary of measured adc reading ranges for each level
        threshold: float -> minimum count (as % of first count) to be accepted as a valid code
        window_width: int -> window-width for calculating running averages
    """
    def __init__(self, buckets, threshold=0.50, window_width=16):
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
        
        f = open("log", "w") # for debugging TODO: Formalize or remove
        for reading in sequence:
            self._window_sum += reading - self._window[self._window_idx]
            self._window_ave = self._window_sum >> self._window_shift_div
            self._window[self._window_idx] = reading

            bucket = self._get_bucket(self._window_ave)
            f.write(str(bucket) + "\n")

            if self._hold_time == -1:
                if bucket == self._bucket_list[-2] and self._seen_peak:
                    self._hold_time = int(self.threshold * self._count)
                    print(self._hold_time,"\n")
                    self._count = 0
                if bucket == self._bucket_list[-1]:
                    self._seen_peak = True
                    self._count += 1

            else:
                # bucket = self._get_bucket(self._window_ave)
                if bucket == self._prev_bucket:
                    self._count += 1
                    if (self._count >= self._hold_time) and (bucket == 1):
                        print("\n")
                        self.reset()
                else:
                    # print(self._count)
                    
                    if self._count >= self._hold_time  and self._prev_bucket%2:
                        print(self._prev_bucket, self._count, self._hold_time)
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

    """
    It ain't much but it's honest work :?

    I wrote this v1 today; Needs a lot of refining but approach shows promise 
    Committing now because it feels like a fitting end to the day :P

    I've tried to be a bit resource conscious in how this is written as this 
    will have to be translated to C: only 1 multiply and no divides :)
    Sliding window is 16 values wide. At 32bits/integer -> 64 bytes
    It's likely that 16bits/integer will do so 32 bytes should be possible
    Now, I'm rambling. 
    """

    buckets = {
    0: (0, 100),
    1: (140, 500),
    2: (1150, 1800),
    3: (2400, 2650),
    4: (2800, 3100)
}
    this_decoder = CountDecoder(buckets)

    with open(r"sample_reads2.txt", "r") as f:
        samples = f.readlines()
        samples = [int(x) for x in samples]

    
    this_decoder.decode(samples)

    
                

                    
                
                
                






        
    



