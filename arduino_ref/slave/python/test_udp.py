#!/usr/bin/env python3
"""Quick UDP test to verify port 5555 is working"""
import socket
import time

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("192.168.4.100", 5555))
sock.settimeout(2)

print(f"Listening on 192.168.4.100:5555")
print("Waiting for packets...")

try:
    for i in range(10):
        try:
            data, addr = sock.recvfrom(1024)
            print(f"Received {len(data)} bytes from {addr}: {data[:20]}")
        except socket.timeout:
            print(f"Timeout {i+1}/10")
except KeyboardInterrupt:
    print("\nStopped")
finally:
    sock.close()
