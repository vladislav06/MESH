import os
import serial
import time
import threading
import json
import sys


def parseConfig(places: dict):
    data = [0x69,0x96]
    data.append(len(places))
    for place in places:
        data.append(place["place"])
        data.append(len(place["usedChannels"]))
        for channel in place["usedChannels"]:
            data.append(ord(channel["var"]))
            data.append(channel["dataCh"])
            data.append(channel["place"])
            data.append(channel["sensorCh"])
            data.append(channel["MixMode"])
        data.append(len(place["algo"]))
        for c in place["algo"]:
            data.append(ord(c))
        data.append(0)
    return data


def main():
    if len(sys.argv) != 3:
        print("Serial port and config file path is expected")
        return

    try:
        ser = serial.Serial(sys.argv[1], 115200)
    except:
        print("Serial port cant be opened")
        return

    if not os.path.isfile(sys.argv[2]):
        print("File port cant be opened")
        return

    with open(sys.argv[2]) as f:
        d = json.load(f)
        data = parseConfig(d)
        ser.write(data)
        ser.flush()
        ser.close()
    return


if __name__ == "__main__":
    main()
