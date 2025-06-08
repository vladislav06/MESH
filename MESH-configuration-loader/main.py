import serial
import time
import threading


# load json configuration
# convert it to byte representation
# send to serial

ser = serial.Serial("/dev/ttyACM0", 115200)

def receive():
    while True:
        bs = ser.readline()
        print(bs)


def transmit():
    while True:
        ser.write(data)
        ser.flush()
        time.sleep(1)


# Send character 'S' to start the program
data = []
for i in range(0, 1024):
    data.append(i%256)
# Read line   


t1 = threading.Thread(target=receive)
t2 = threading.Thread(target=transmit)

t1.start()
t2.start()
