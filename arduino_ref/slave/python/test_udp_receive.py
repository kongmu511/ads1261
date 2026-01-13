#!/usr/bin/env python3
"""Test if UDP packets can be received on 192.168.4.100:5555"""
import socket
import time

print("Testing UDP reception on 192.168.4.100:5555")

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

try:
    sock.bind(("192.168.4.100", 5555))
    print("✓ Bound to 192.168.4.100:5555")
    sock.settimeout(1)
    
    print("Listening for packets for 10 seconds...")
    start_time = time.time()
    packet_count = 0
    
    while (time.time() - start_time) < 10:
        try:
            data, addr = sock.recvfrom(1024)
            packet_count += 1
            print(f"Packet {packet_count}: {len(data)} bytes from {addr}")
            if packet_count == 1:
                print(f"First packet data: {data[:20]}")
        except socket.timeout:
            print(".", end="", flush=True)
    
    print(f"\nReceived {packet_count} packets in 10 seconds")
    
except Exception as e:
    print(f"Error: {e}")
finally:
    sock.close()
    print("Socket closed")