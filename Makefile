PYTHON = python3
PORT ?= /dev/tty.usbmodem14101
BAUD ?= 921600

.PHONY: build flash monitor clean calibrate config plot help all

help:
	@echo "GRF Force Platform - Build & Deployment"
	@echo "========================================"
	@echo "make build           - Build the project"
	@echo "make flash           - Build and flash"
	@echo "make monitor         - Start serial monitor"
	@echo "make flash-monitor   - Build, flash, and monitor"
	@echo "make plot            - Start real-time serial plotter (macOS/Linux)"
	@echo "make clean           - Clean artifacts"
	@echo "make config          - ESP-IDF menuconfig"
	@echo "make calibrate       - Run calibration tool"
	@echo "make calibrate-test  - Test calibration tool"
	@echo ""
	@echo "Optional: PORT=/dev/ttyACM0 BAUD=115200"
	@echo "For macOS, typically: PORT=/dev/tty.usbmodem1*, example: PORT=/dev/tty.usbmodem14101"
	@echo "Find your port with: ls /dev/tty.usbmodem*"

build:
	idf.py build

flash: build
	@echo "Flashing to port: $(PORT)"
	@echo "If the port is not found, connect your ESP32 and run 'ls /dev/tty.usbmodem*' to find the correct port"
	idf.py -p $(PORT) flash

monitor:
	@echo "Connecting to port: $(PORT)"
	idf.py -p $(PORT) monitor --baud $(BAUD)

flash-monitor: build
	@echo "Flashing to port: $(PORT)"
	@echo "If the port is not found, connect your ESP32 and run 'ls /dev/tty.usbmodem*' to find the correct port"
	idf.py -p $(PORT) build flash monitor --baud $(BAUD)

plot:
	@echo "Starting real-time serial plotter (macOS/Linux/Windows compatible)..."
	python3 serial_plotter.py --port $(PORT) --baud $(BAUD)

clean:
	idf.py fullclean

config:
	idf.py menuconfig

calibrate:
	python3 calibration_tool.py

calibrate-test:
	python3 calibration_tool.py --test

all: build flash monitor