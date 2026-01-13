#!/usr/bin/env python3
"""
Real-time plot for ADS1261 4-channel data
Displays live plotting with matplotlib
"""

import serial
import struct
import sys
from datetime import datetime
from collections import deque
import threading

try:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not installed. Install with: pip3 install matplotlib")

class ChannelMonitor:
    def __init__(self, port, baudrate=115200, max_points=200):
        self.port = port
        self.baudrate = baudrate
        self.max_points = max_points
        
        # Data buffers
        self.timestamps = deque(maxlen=max_points)
        self.ch1_data = deque(maxlen=max_points)
        self.ch2_data = deque(maxlen=max_points)
        self.ch3_data = deque(maxlen=max_points)
        self.ch4_data = deque(maxlen=max_points)
        
        self.serial_port = None
        self.running = False
        self.frame_count = 0
        
    def connect(self):
        try:
            self.serial_port = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            self.running = True
        except serial.SerialException as e:
            print(f"Failed to connect: {e}")
            sys.exit(1)
    
    def read_data(self):
        """Read data from serial in background thread"""
        while self.running:
            try:
                # Look for header byte (0xAA)
                header = self.serial_port.read(1)
                if not header:
                    continue
                
                if header[0] != 0xAA:
                    continue
                
                # Read 4 float values (16 bytes)
                data = self.serial_port.read(16)
                if len(data) < 16:
                    continue
                
                # Parse 4 floats (little-endian)
                try:
                    ch1, ch2, ch3, ch4 = struct.unpack('<ffff', data)
                    self.timestamps.append(len(self.timestamps))
                    self.ch1_data.append(ch1)
                    self.ch2_data.append(ch2)
                    self.ch3_data.append(ch3)
                    self.ch4_data.append(ch4)
                    self.frame_count += 1
                    
                    # Print to console every 10 frames
                    if self.frame_count % 10 == 0:
                        print(f"[{self.frame_count}] CH1:{ch1:.4f} CH2:{ch2:.4f} CH3:{ch3:.4f} CH4:{ch4:.4f}")
                except struct.error:
                    continue
                    
            except Exception as e:
                print(f"Read error: {e}")
                break
    
    def start_reading(self):
        """Start background thread for reading"""
        read_thread = threading.Thread(target=self.read_data, daemon=True)
        read_thread.start()
        return read_thread
    
    def update_plot(self, frame, ax1, ax2):
        """Update plot data"""
        ax1.clear()
        ax2.clear()
        
        if self.timestamps:
            # Plot 1: All channels
            ax1.plot(self.timestamps, self.ch1_data, 'b-', label='CH1', linewidth=1.5)
            ax1.plot(self.timestamps, self.ch2_data, 'r-', label='CH2', linewidth=1.5)
            ax1.plot(self.timestamps, self.ch3_data, 'g-', label='CH3', linewidth=1.5)
            ax1.plot(self.timestamps, self.ch4_data, 'orange', label='CH4', linewidth=1.5)
            
            ax1.set_xlabel('Sample')
            ax1.set_ylabel('Value')
            ax1.set_title(f'ADS1261 Real-time Data (Frames: {self.frame_count})')
            ax1.legend(loc='upper left')
            ax1.grid(True, alpha=0.3)
            
            # Plot 2: Current values
            channels = ['CH1', 'CH2', 'CH3', 'CH4']
            values = [
                self.ch1_data[-1] if self.ch1_data else 0,
                self.ch2_data[-1] if self.ch2_data else 0,
                self.ch3_data[-1] if self.ch3_data else 0,
                self.ch4_data[-1] if self.ch4_data else 0,
            ]
            colors = ['blue', 'red', 'green', 'orange']
            ax2.bar(channels, values, color=colors, alpha=0.7)
            ax2.set_ylabel('Current Value')
            ax2.set_title('Current Channel Values')
            ax2.grid(True, alpha=0.3, axis='y')
            
            # Auto scale y-axis
            if any(values):
                max_val = max(abs(v) for v in values) * 1.2
                ax2.set_ylim(-max_val, max_val)
    
    def plot(self):
        """Start live plotting"""
        if not MATPLOTLIB_AVAILABLE:
            print("matplotlib required for plotting. Install with: pip3 install matplotlib")
            return
        
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
        fig.suptitle('ADS1261 4-Channel Monitor')
        
        ani = animation.FuncAnimation(fig, self.update_plot, fargs=(ax1, ax2),
                                    interval=100, cache_frame_data=False)
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print(f"\nTotal frames: {self.frame_count}")
            self.running = False
    
    def close(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()

def find_serial_port():
    """Auto-detect ESP32 serial port on macOS"""
    import glob
    ports = glob.glob('/dev/tty.usb*') + glob.glob('/dev/tty.SLAB*') + glob.glob('/dev/tty.wchusbserial*')
    if ports:
        return ports[0]
    return None

def main():
    print("=== ADS1261 Real-time Plot Monitor ===\n")
    
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
    
    # Create monitor
    monitor = ChannelMonitor(port, baudrate)
    monitor.connect()
    
    # Start background reading thread
    monitor.start_reading()
    
    # Start plotting
    try:
        monitor.plot()
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        monitor.close()

if __name__ == "__main__":
    main()
