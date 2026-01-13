#!/usr/bin/env python3
"""Just listen for UDP packets - no commands"""

import socket
import struct
import time

LOCAL_IP = "192.168.4.100"
UDP_PORT = 5555
TIMEOUT = 5  # seconds

print("\n=== Simple UDP Listener ===")
print(f"Listening on {LOCAL_IP}:{UDP_PORT} for {TIMEOUT} seconds...")
print("(ESP32 should auto-stream, no command needed)\n")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((LOCAL_IP, UDP_PORT))
sock.settimeout(0.1)

start = time.time()
count = 0

try:
    while (time.time() - start) < TIMEOUT:
        try:
            data, addr = sock.recvfrom(64)
            count += 1
            if count == 1:
                print(f"✓ First packet received from {addr}!")
                if len(data) == 20:
                    values = struct.unpack("<Iiiii", data)
                    print(f"  Timestamp: {values[0]}")
                    print(f"  CH1: {values[1]}")
                    print(f"  CH2: {values[2]}")
                    print(f"  CH3: {values[3]}")
                    print(f"  CH4: {values[4]}")
            elif count % 500 == 0:
                print(f"  ... {count} packets so far")
        except socket.timeout:
            continue
except KeyboardInterrupt:
    pass

sock.close()

if count > 0:
    rate = count / (time.time() - start)
    print(f"\n✓ SUCCESS: Received {count} packets at {rate:.1f} Hz\n")
else:
    print("\n✗ NO PACKETS - Check ESP32 serial monitor\n")
