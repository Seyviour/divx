import serial


serial_params = {
    "port": "/dev/ttyUSB0",
    "baudrate": 115200,
}


ser = serial.Serial(**serial_params)

x=0
with open("sample_reads2.txt", "w") as f:
    while (x<10000):
        x+=1
        try:
            data = ser.readline()
            data = data.decode()
            f.write(data)
        except:
            continue 
