import serial
import struct
import time

ser = serial.Serial("COM10", 2000000, timeout=1)
time.sleep(2)
ser.reset_input_buffer()

print("Reading packets...")
for i in range(10):
    data = ser.read(20)
    if len(data) == 20:
        vals = struct.unpack("<Iiiii", data)
        print(f"CH1:{vals[1]:10d} CH2:{vals[2]:10d} CH3:{vals[3]:10d} CH4:{vals[4]:10d}")
    time.sleep(0.1)

ser.close()
print("Done")
