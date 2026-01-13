#!/usr/bin/env python3
"""
Serial receiver for ZPlate load cell data
Reads binary packets and displays force values
"""

import serial
import struct
import time
from collections import deque

# Configuration
SERIAL_PORT = "COM10"
BAUD_RATE = 2000000
PACKET_SIZE = 20
CALIB_FACTOR = 2231.19  # ADC counts per Newton

def main():
    print("\n=== ZPlate Serial Receiver ===\n")
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✓ Connected to {SERIAL_PORT} at {BAUD_RATE} baud")
        time.sleep(2)  # Wait for ESP32 to start
        
        # Clear any initial text output
        ser.reset_input_buffer()
        
        print("\nReceiving data... (Ctrl+C to stop)\n")
        
        # Buffers for averaging
        buffers = {
            'ch1': deque(maxlen=100),
            'ch2': deque(maxlen=100),
            'ch3': deque(maxlen=100),
            'ch4': deque(maxlen=100)
        }
        
        packet_count = 0
        last_display = time.time()
        
        while True:
            # Read packet
            data = ser.read(PACKET_SIZE)
            
            if len(data) == PACKET_SIZE:
                try:
                    # Parse: uint32 + 4x int32
                    timestamp, ch1, ch2, ch3, ch4 = struct.unpack("<Iiiii", data)
                    
                    # Add to buffers
                    buffers['ch1'].append(ch1)
                    buffers['ch2'].append(ch2)
                    buffers['ch3'].append(ch3)
                    buffers['ch4'].append(ch4)
                    
                    packet_count += 1
                    
                    # Display every 0.5 seconds
                    if time.time() - last_display > 0.5 and len(buffers['ch1']) > 10:
                        # Calculate averages
                        avg1 = sum(buffers['ch1']) / len(buffers['ch1'])
                        avg2 = sum(buffers['ch2']) / len(buffers['ch2'])
                        avg3 = sum(buffers['ch3']) / len(buffers['ch3'])
                        avg4 = sum(buffers['ch4']) / len(buffers['ch4'])
                        
                        # Convert to Newtons
                        n1 = avg1 / CALIB_FACTOR
                        n2 = avg2 / CALIB_FACTOR
                        n3 = avg3 / CALIB_FACTOR
                        n4 = avg4 / CALIB_FACTOR
                        
                        # Find which channel is different (loaded)
                        values = [avg1, avg2, avg3, avg4]
                        range_val = max(values) - min(values)
                        
                        print(f"\r[{packet_count:5d} packets]  ", end="")
                        print(f"CH1: {n1:7.2f}N  ", end="")
                        print(f"CH2: {n2:7.2f}N  ", end="")
                        print(f"CH3: {n3:7.2f}N  ", end="")
                        print(f"CH4: {n4:7.2f}N  ", end="")
                        
                        if range_val > 1000:
                            # Determine loaded channel
                            sorted_vals = sorted(values)
                            for i, (name, val) in enumerate([('CH1', avg1), ('CH2', avg2), ('CH3', avg3), ('CH4', avg4)], 1):
                                if abs(val - sorted_vals[0]) < 100 or abs(val - sorted_vals[3]) > 100:
                                    print(f" ← {name} LOADED", end="")
                                    break
                        elif range_val < 10:
                            print(" ← ALL SAME (ADC issue?)", end="")
                        
                        print("    ", end="", flush=True)
                        last_display = time.time()
                        
                except struct.error:
                    # Ignore malformed packets
                    pass
            else:
                # Resync if packet incomplete
                if len(data) > 0:
                    ser.reset_input_buffer()
    
    except serial.SerialException as e:
        print(f"\n✗ Serial error: {e}")
        print(f"   Check: Is {SERIAL_PORT} the correct port?")
        print(f"   Check: Is ESP32 connected?")
    except KeyboardInterrupt:
        print("\n\n✓ Stopped")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
