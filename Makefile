PYTHON = python3
PORT ?= /dev/ttyUSB0
BAUD ?= 921600

.PHONY: build flash monitor clean calibrate config help all

help:
	@echo "GRF Force Platform - Build & Deployment"
	@echo "========================================"
	@echo "make build           - Build the project"
	@echo "make flash           - Build and flash"
	@echo "make monitor         - Start serial monitor"
	@echo "make flash-monitor   - Build, flash, and monitor"
	@echo "make clean           - Clean artifacts"
	@echo "make config          - ESP-IDF menuconfig"
	@echo "make calibrate       - Run calibration tool"
	@echo "make calibrate-test  - Test calibration tool"
	@echo ""
	@echo "Optional: PORT=/dev/ttyACM0 BAUD=115200"

build:
	idf.py build

flash: build
	idf.py -p $(PORT) flash

monitor:
	idf.py -p $(PORT) monitor --baud $(BAUD)

flash-monitor: build
	idf.py -p $(PORT) build flash monitor --baud $(BAUD)

clean:
	idf.py fullclean

config:
	idf.py menuconfig

calibrate:
	python3 calibration_tool.py

calibrate-test:
	python3 calibration_tool.py --test

all: build flash monitor
