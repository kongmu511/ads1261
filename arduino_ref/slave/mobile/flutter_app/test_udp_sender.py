#!/usr/bin/env python3
"""Small UDP test sender for ZPlate mobile/desktop apps.

Usage:
    python test_udp_sender.py --ip 192.168.4.1 --port 5555 --rate 100

Sends 20-byte little-endian packets matching the desktop/mobile layout:
    uint32 timestamp_us
    int32 ch1
    int32 ch2
    int32 ch3
    int32 ch4
"""
import argparse
import socket
import struct
import time
import random

parser = argparse.ArgumentParser()
parser.add_argument("--ip", default="127.0.0.1")
parser.add_argument("--port", type=int, default=5555)
parser.add_argument("--rate", type=float, default=100.0, help="samples per second per packet")
args = parser.parse_args()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
seq = 0
try:
    while True:
        ts = int(time.time() * 1_000_000) & 0xFFFFFFFF
        ch = [int((random.random() - 0.5) * 1e6) for _ in range(4)]
        pkt = struct.pack('<Iiiii', ts, ch[0], ch[1], ch[2], ch[3])
        sock.sendto(pkt, (args.ip, args.port))
        seq += 1
        time.sleep(1.0 / args.rate)
except KeyboardInterrupt:
    pass

print('Finished')
