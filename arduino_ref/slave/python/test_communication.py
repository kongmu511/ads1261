#!/usr/bin/env python3
"""
Step-by-step communication test for ZPlate system
Tests each part of the communication chain individually
"""

import socket
import struct
import time

def test_1_send_command():
    """Test 1: Send START command to ESP32"""
    print("=== TEST 1: Send START command ===")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        command = b'S'
        result = sock.sendto(command, ("192.168.4.1", 5555))
        sock.close()
        print(f"✓ Sent START command ({result} bytes)")
        print("  Check serial monitor for '▶ Streaming STARTED'")
        return True
    except Exception as e:
        print(f"✗ Failed to send command: {e}")
        return False

def test_2_receive_packets():
    """Test 2: Listen for data packets from ESP32"""
    print("\n=== TEST 2: Listen for data packets ===")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("192.168.4.100", 5555))
        sock.settimeout(5)
        print("✓ Listening on 192.168.4.100:5555")
        
        packet_count = 0
        start_time = time.time()
        
        while packet_count < 10 and (time.time() - start_time) < 5:
            try:
                data, addr = sock.recvfrom(64)
                packet_count += 1
                print(f"  Packet {packet_count}: {len(data)} bytes from {addr}")
                
                if len(data) == 20:
                    try:
                        timestamp, ch1, ch2, ch3, ch4 = struct.unpack("<Iiiii", data)
                        print(f"    Timestamp: {timestamp}, CH1: {ch1}")
                    except:
                        print(f"    Raw data: {data.hex()}")
                else:
                    print(f"    Unexpected size: {data}")
                    
            except socket.timeout:
                break
        
        sock.close()
        if packet_count > 0:
            print(f"✓ Received {packet_count} packets")
            return True
        else:
            print("✗ No packets received")
            return False
            
    except Exception as e:
        print(f"✗ Failed to receive packets: {e}")
        return False

def test_3_stop_command():
    """Test 3: Send STOP command"""
    print("\n=== TEST 3: Send STOP command ===")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        command = b'E'
        result = sock.sendto(command, ("192.168.4.1", 5555))
        sock.close()
        print(f"✓ Sent STOP command ({result} bytes)")
        print("  Check serial monitor for '■ Streaming STOPPED'")
        return True
    except Exception as e:
        print(f"✗ Failed to send STOP: {e}")
        return False

def main():
    print("ZPlate Communication Test")
    print("=" * 40)
    print("Prerequisites:")
    print("- PC connected to ZPlate WiFi (IP: 192.168.4.100)")
    print("- ESP32 running and showing 'Ready! Waiting for START command'")
    print("- Serial monitor open to see ESP32 responses")
    print()
    
    # Test sequence
    input("Press Enter to start Test 1 (Send START)...")
    test1_ok = test_1_send_command()
    
    if test1_ok:
        time.sleep(1)
        input("\nPress Enter to start Test 2 (Receive data)...")
        test2_ok = test_2_receive_packets()
        
        time.sleep(1)
        input("\nPress Enter to start Test 3 (Send STOP)...")
        test3_ok = test_3_stop_command()
        
        print("\n" + "=" * 40)
        print("SUMMARY:")
        print(f"Send START:     {'✓' if test1_ok else '✗'}")
        print(f"Receive data:   {'✓' if test2_ok else '✗'}")
        print(f"Send STOP:      {'✓' if test3_ok else '✗'}")
        
        if test1_ok and test2_ok and test3_ok:
            print("\n🎉 All tests passed! Communication working.")
        elif test1_ok and test3_ok and not test2_ok:
            print("\n⚠ Commands work but no data received.")
            print("   Possible causes:")
            print("   - ESP32 ADC not reading properly")
            print("   - Firewall blocking UDP data packets")
            print("   - Timer not triggering at 1000 Hz")
        else:
            print("\n❌ Communication failed.")
    else:
        print("\n❌ Basic command sending failed.")

if __name__ == "__main__":
    main()