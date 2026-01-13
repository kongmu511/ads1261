#!/usr/bin/env python3
"""Simple test to verify ESP32 communication and detect loaded cell"""

import socket
import struct
import time
from collections import defaultdict

# Configuration
ESP32_IP = "192.168.4.1"
LOCAL_IP = "192.168.4.100"
UDP_PORT = 5555
CALIB_FACTOR = 2231.19
TEST_DURATION = 3  # seconds

def send_command(cmd):
    """Send single-byte command to ESP32"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(cmd.encode(), (ESP32_IP, UDP_PORT))
    sock.close()
    print(f"  → Sent '{cmd}' command")

def main():
    print("\n=== ZPlate Quick Test ===\n")
    
    # Test 1: Send START
    print("[1/3] Sending START command...")
    send_command('S')
    time.sleep(0.5)
    
    # Test 2: Receive packets
    print(f"[2/3] Listening on {LOCAL_IP}:{UDP_PORT} for {TEST_DURATION}s...")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((LOCAL_IP, UDP_PORT))
    sock.settimeout(0.1)
    
    packets = []
    start = time.time()
    
    try:
        while (time.time() - start) < TEST_DURATION:
            try:
                data, addr = sock.recvfrom(64)
                if len(data) == 20:
                    # Parse: uint32 + 4x int32
                    values = struct.unpack("<Iiiii", data)
                    packets.append({
                        'timestamp': values[0],
                        'ch1': values[1],
                        'ch2': values[2],
                        'ch3': values[3],
                        'ch4': values[4]
                    })
            except socket.timeout:
                continue
    except KeyboardInterrupt:
        pass
    
    sock.close()
    
    # Test 3: Analyze
    print(f"[3/3] Analysis\n")
    
    if not packets:
        print("  ✗ NO PACKETS RECEIVED!")
        print("    Check: ESP32 serial monitor - is it streaming?")
        print("    Check: Firewall blocking port 5555?")
        print("    Check: IP address 192.168.4.100 configured?")
        return
    
    print(f"  ✓ Received {len(packets)} packets")
    
    # Calculate averages
    avg = {
        'ch1': sum(p['ch1'] for p in packets) / len(packets),
        'ch2': sum(p['ch2'] for p in packets) / len(packets),
        'ch3': sum(p['ch3'] for p in packets) / len(packets),
        'ch4': sum(p['ch4'] for p in packets) / len(packets)
    }
    
    # Show results
    print("\n  ADC Values (Raw → Newtons):")
    for ch in ['ch1', 'ch2', 'ch3', 'ch4']:
        raw = int(avg[ch])
        newtons = avg[ch] / CALIB_FACTOR
        print(f"    {ch.upper()}: {raw:11d} → {newtons:8.2f} N")
    
    # Find loaded cell
    values = [avg['ch1'], avg['ch2'], avg['ch3'], avg['ch4']]
    range_val = max(values) - min(values)
    
    print(f"\n  Range: {int(range_val)} ADC counts")
    
    if range_val > 1000:
        print("  ✓ LOAD DETECTED!")
        # Find which channel is different
        sorted_vals = sorted(values)
        for i, ch in enumerate(['ch1', 'ch2', 'ch3', 'ch4'], 1):
            if abs(avg[ch] - sorted_vals[0]) < 100 or abs(avg[ch] - sorted_vals[3]) > 100:
                print(f"    → CH{i} appears to be loaded")
    elif range_val < 10:
        print("  ⚠ All channels identical - ADC not reading?")
    else:
        print("  ⚠ Small variation (noise or light load)")
    
    # Sample rate
    duration = (packets[-1]['timestamp'] - packets[0]['timestamp']) / 1_000_000
    rate = len(packets) / duration if duration > 0 else 0
    print(f"\n  Sample Rate: {rate:.1f} Hz")
    
    # Send STOP
    print("\nSending STOP command...")
    send_command('E')
    print("Done!\n")

if __name__ == "__main__":
    main()
