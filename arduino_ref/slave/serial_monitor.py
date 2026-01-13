#!/usr/bin/env python3
"""
Serial monitor for ADS1261 4-channel data
Reads and displays real-time data from ESP32-C6
"""

import serial
import struct
import sys
from datetime import datetime

def find_serial_port():
    """Auto-detect ESP32 serial port on macOS"""
    import glob
    ports = glob.glob('/dev/tty.usb*') + glob.glob('/dev/tty.SLAB*') + glob.glob('/dev/tty.wchusbserial*')
    if ports:
        return ports[0]
    return None

def read_channel_data(port, baudrate=115200):
    """
    Read and parse channel data from serial port
    Expected format: 0xAA (header) + ch1 (float) + ch2 (float) + ch3 (float) + ch4 (float)
    Total: 17 bytes per frame
    """
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"Connected to {port} at {baudrate} baud")
        print("Waiting for data (0xAA header)...\n")
        print(f"{'Time':<12} {'CH1':<12} {'CH2':<12} {'CH3':<12} {'CH4':<12}")
        print("-" * 60)
        
        frame_count = 0
        while True:
            # Look for header byte (0xAA)
            header = ser.read(1)
            if not header:
                continue
            
            if header[0] != 0xAA:
                continue
            
            # Read 4 float values (16 bytes)
            data = ser.read(16)
            if len(data) < 16:
                continue
            
            # Parse 4 floats (little-endian)
            try:
                ch1, ch2, ch3, ch4 = struct.unpack('<ffff', data)
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                print(f"{timestamp}  {ch1:<12.4f} {ch2:<12.4f} {ch3:<12.4f} {ch4:<12.4f}")
                frame_count += 1
            except struct.error:
                continue
                
    except serial.SerialException as e:
        print(f"Serial Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n\nTotal frames received: {frame_count}")
        print("Exiting...")
        ser.close()
        sys.exit(0)

def main():
    print("=== ADS1261 4-Channel Serial Monitor ===\n")
    
    # Try to auto-detect port
    port = find_serial_port()
    
    if not port:
        print("Available serial ports:")
        import glob
        ports = glob.glob('/dev/tty.*')
        if ports:
            for p in ports:
                print(f"  {p}")
        else:
            print("  No serial ports found!")
        
        port = input("\nEnter serial port (e.g., /dev/tty.usbserial-xxx): ").strip()
        if not port:
            print("No port specified. Exiting.")
            sys.exit(1)
    
    baudrate = 115200
    user_baud = input(f"Enter baudrate [{baudrate}]: ").strip()
    if user_baud:
        try:
            baudrate = int(user_baud)
        except ValueError:
            print(f"Invalid baudrate, using default {baudrate}")
    
    print()
    read_channel_data(port, baudrate)

if __name__ == "__main__":
    main()
