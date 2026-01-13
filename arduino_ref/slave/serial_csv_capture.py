#!/usr/bin/env python3
"""
Capture CSV data from ESP32-C6 serial output
Saves timestamped ADC data to a CSV file for analysis
"""
import serial
import time
import sys

PORT = 'COM10'
BAUD = 921600
OUTPUT_FILE = 'adc_data.csv'

print(f"Opening {PORT} at {BAUD} baud...")
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(2)  # Wait for ESP32 to reset
    
    print("Waiting for CSV data...")
    csv_started = False
    line_count = 0
    
    with open(OUTPUT_FILE, 'w') as f:
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # Detect CSV start
                if "Timestamp_us,CH1,CH2,CH3,CH4" in line:
                    csv_started = True
                    f.write(line + '\n')
                    print(f"CSV header detected, capturing...")
                    continue
                
                # Capture CSV data lines
                if csv_started:
                    if line.startswith("---"):  # End marker
                        print(f"CSV capture complete: {line_count} samples")
                        break
                    
                    # Check if line contains comma-separated numbers
                    if ',' in line and line[0].isdigit():
                        f.write(line + '\n')
                        line_count += 1
                        if line_count % 100 == 0:
                            print(f"  Captured {line_count} samples...")
                else:
                    # Print non-CSV lines for debugging
                    if line:
                        print(f"  {line}")
            
            time.sleep(0.001)
    
    ser.close()
    print(f"\nData saved to {OUTPUT_FILE}")
    print(f"Total samples: {line_count}")
    
except serial.SerialException as e:
    print(f"Error: {e}")
    sys.exit(1)
except KeyboardInterrupt:
    print("\nCapture interrupted by user")
    ser.close()
