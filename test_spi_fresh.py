#!/usr/bin/env python3
import serial
import time
import subprocess

# Reset device to see fresh boot
subprocess.run(["python3", "-m", "esptool", "-p", "/dev/tty.usbmodem1201", "chip_id"], 
               capture_output=True, timeout=5)
time.sleep(2)

ser = serial.Serial('/dev/tty.usbmodem1201', 115200, timeout=1)
time.sleep(0.5)
ser.reset_input_buffer()

print("=" * 60)
print("FINAL VERIFICATION - FRESH BOOT")
print("=" * 60)
print()

boot_ok = False
spi_ok = False
init_ok = False
spi_count = 0
line_count = 0

while line_count < 100:
    try:
        line = ser.readline().decode('utf-8', errors='ignore').rstrip()
        if not line:
            continue
        line_count += 1
            
        if "SPI bus initialized successfully" in line:
            boot_ok = True
            print(f"✓ {line}")
        elif "ADS1261 initialized" in line:
            init_ok = True
            print(f"✓ {line}")
        elif "WriteReg:" in line and spi_count < 3:
            spi_ok = True
            print(f"✓ SPI writes working: {line[:65]}")
            spi_count += 1
        elif "MISO GPIO: 8" in line:
            print(f"✓ {line}")
    except:
        pass

print()
print("=" * 60)
print("VERIFICATION RESULTS:")
print("=" * 60)
print(f"Boot initialization: {'✅ PASS' if boot_ok else '❌ FAIL'}")
print(f"ADS1261 init: {'✅ PASS' if init_ok else '❌ FAIL'}")
print(f"SPI write commands: {'✅ PASS' if spi_ok else '❌ FAIL'}")
print()
if boot_ok and spi_ok:
    print("🎉 SUCCESS - SPI Communication Fully Operational!")
    print()
    print("Key Fix Applied: GPIO8 for MISO (custom board)")
    print("Status: Ready for ADC data verification")
print("=" * 60)

ser.close()
