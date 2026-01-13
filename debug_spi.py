#!/usr/bin/env python3
import serial
import time

ser = serial.Serial('/dev/tty.usbmodem1201', 115200, timeout=1)
time.sleep(1)
ser.reset_input_buffer()
time.sleep(2)

print("=" * 70)
print("DETAILED SPI DEBUG OUTPUT - ADS1261 DATA ANALYSIS")
print("=" * 70)
print()

for i in range(200):
    try:
        line = ser.readline().decode('utf-8', errors='ignore').rstrip()
        if line and ('SPI:' in line or 'ReadReg' in line or 'WriteReg' in line):
            print(line)
    except:
        pass

ser.close()
