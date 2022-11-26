# Week 1
## Sensor Testing - QRD1114

I got two QRD1114 sensors about 2 months ago when I first decided to begin work on this project. It's funny how they were the last two in the store when I got them. A rare instance of me being lucky!

I followed a [Sparkfun tutorial](https://learn.sparkfun.com/tutorials/qrd1114-optical-detector-hookup-guide/all) to set up a sensor for the circuit. I desperately need an electronics refresher, smh.

I tried out different setups for the tests (I'll fill in more info here later). I settled on a pen form-factor. 

Results with the sensor in the pen form-factor were a lot more consistent and promising than the previous results -- enough to say that it is possible to identify different shades of gray consistently (in somewhat controlled conditions, at the very least).


## Setup
The shades of gray used for testing were obtained from Wikimedia, and are available [here](https://upload.wikimedia.org/wikipedia/commons/2/29/50-shades-grey.svg). I printed a page of the mentioned document out and cut out strips from the first column of each row. The cutouts were labeled 0-6 (from lightest to darkest). The VCC measurements were made with the sensor-pen placed over the strips. 


<img src="https://upload.wikimedia.org/wikipedia/commons/2/29/50-shades-grey.svg" width="400" height="400">

| Cutout |VCC(Volts)||||
|--------|---|---|---|---|
|        |pass1   |pass2  |pass3  |pass4|
|0       |0.47    |0.47   |0.15   |0.93|
|1       |0.51    |0.50   |0.17   |1.48|
|2       |0.65    |0.63   |0.21   |1.80|
|3       |1.10    |1.08   |0.38   |2.03|
|4       |1.50    |1.48   |0.94   |2.18|
|5       |1.76    |1.71   |1.33   |2.31|
|6       |1.93    |1.90   |1.60   |2.34|  

There is a clear pattern here. 
The control I have on the distance of the sheet from the sensor is not perfect (as evident in the pass 3 and pass 4 readings). I'm reluctant to glue anything in place since I have just two sensors. 


## Onward
### MCU
A RISC-V based MCU would be ideal; but it'll take at least 3 weeks to get one if I order from AliExpress right now. I can't wait that long so I'll try to get an ESP-32 to work with, for now. Porting C/C++ code to a RISC-V MCU (when it's available) shouldn't be difficult.

### Photo-diodes
Photodiodes are supposed to be more sensitive than phototransistors. I'd love to experiment with them but I don't think I'll be able to get any in the next 3-weeks. In any case, I believe the work done with a photo-transistor setup will carry over nicely to a photo-diode setup.