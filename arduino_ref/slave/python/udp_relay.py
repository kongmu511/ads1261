#!/usr/bin/env python3
"""
UDP Relay - Forwards commands to ESP32 and relays data back.
Run this to bridge the IP mismatch (PC is 192.168.4.2, ESP32 expects 192.168.4.100)
"""

import socket
import sys

# Configuration
ESP32_IP = "192.168.4.1"
ESP32_PORT = 5555
RELAY_PORT = 5555  # GUI connects to this port on localhost

def main():
    # Create two sockets
    # Socket 1: Bind to 192.168.4.100:5555 to receive ESP32 data
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receiver.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    receiver.bind(("192.168.4.100", ESP32_PORT))  # Pretend to be .100
    receiver.setblocking(False)
    
    # Socket 2: Listen for GUI commands and send to ESP32
    relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    relay.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    relay.bind(("127.0.0.1", RELAY_PORT))
    relay.setblocking(False)
    
    # Socket 3: Send to ESP32
    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"UDP Relay started!")
    print(f"Listening for ESP32 data on 192.168.4.100:{ESP32_PORT}")
    print(f"GUI should connect to 127.0.0.1:{RELAY_PORT}")
    print(f"Forwarding commands to {ESP32_IP}:{ESP32_PORT}")
    print("Press Ctrl+C to stop\n")
    
    gui_addr = None
    packet_count = 0
    
    try:
        while True:
            # Receive data from ESP32
            try:
                data, addr = receiver.recvfrom(64)
                if len(data) == 20:  # Data packet
                    packet_count += 1
                    if gui_addr:
                        relay.sendto(data, gui_addr)
                    if packet_count % 1000 == 0:
                        print(f"Relayed {packet_count} packets from ESP32 to GUI")
            except BlockingIOError:
                pass
            
            # Receive commands from GUI
            try:
                cmd, addr = relay.recvfrom(64)
                gui_addr = addr  # Remember GUI address
                # Forward to ESP32
                sender.sendto(cmd, (ESP32_IP, ESP32_PORT))
                print(f"Command '{cmd.decode()}' forwarded to ESP32")
            except BlockingIOError:
                pass
                
    except KeyboardInterrupt:
        print(f"\nRelay stopped. Total packets: {packet_count}")
        receiver.close()
        relay.close()
        sender.close()

if __name__ == "__main__":
    main()
