import array
import os
import serial
import time
import threading
import json
import sys


def main():
    if len(sys.argv) <3:
        print("Serial port and config file path is expected")
        return

    try:
        ser = serial.Serial(sys.argv[1], 115200)
    except:

        print("Serial port cant be opened")
        return

    data = array.array('B', [])
    data.append(0x69)
    data.append(0x97)
    # place
    data.append(int(sys.argv[2]))
    # sensorCh
    data.append(int(sys.argv[3]))

    for i in range(4, len(sys.argv)):
        data.append(int(sys.argv[i]))

    for i in range(5):
        data.append(0)
    ser.write(data)
    ser.flush()
    ser.close()
    return


if __name__ == "__main__":
    main()
