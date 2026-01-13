#!/usr/bin/env python3
import serial
import time

ser = serial.Serial('/dev/tty.usbmodem1201', 115200, timeout=1)
time.sleep(1)
ser.reset_input_buffer()
time.sleep(2)

print("=" * 60)
print("FINAL VERIFICATION TEST - ESP32-C6 ADS1261 SPI FIX")
print("=" * 60)
print()

spi_count = 0
boot_ok = False
init_ok = False

for i in range(100):
    try:
        line = ser.readline().decode('utf-8', errors='ignore').rstrip()
        if not line:
            continue
            
        if "SPI bus initialized successfully" in line:
            boot_ok = True
            print(f"✓ {line}")
        elif "ADS1261 initialized" in line:
            init_ok = True
            print(f"✓ {line}")
        elif "WriteReg:" in line:
            spi_count += 1
            if spi_count == 1:
                print(f"✓ First SPI write: {line[:70]}")
    except:
        pass

print()
print(f"Total WriteReg commands: {spi_count}")
print()
print("=" * 60)
if boot_ok and init_ok and spi_count > 5:
    print("✅ SUCCESS! SPI Communication is WORKING!")
    print("    - Boot sequence: OK")
    print("    - ADS1261 initialized: OK")
    print(f"    - SPI transactions: {spi_count} commands")
else:
    print("⚠️  Some components not initialized properly")
print("=" * 60)

ser.close()
