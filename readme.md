Using ESP32C6 and TI's ADS1261 ADC to measure four loadcells.

## Loadcell Design
- Use ADS1261's differential mode to connect loadcells to differential inputs
- Use excitation voltage as external reference voltage (ratiometric measurement)
- Configure PGA gain to 128 with high output data rate

## References
- TI's ADS1261 datasheet https://www.ti.com/document-viewer/ads1261/datasheet
- TI Precision Lab's GitHub repositories. https://github.com/TexasInstruments/precision-adc-examples/tree/main/devices/ads1261
- ADS1261 sample code

## Hardware and Toolchain

* ESP32C6
* TI ADS1261 ADC
* TI Precision Lab GitHub repositories
* ESP-IDF
* C
* CMake
* GCC

## Project Structure
```
ads1261/
├── readme.md                          # Project overview
├── HARDWARE_SETUP.md                  # Pin configuration and wiring guide
├── CMakeLists.txt                     # Root CMake configuration
├── Makefile                           # Build convenience commands
├── sdkconfig                          # ESP-IDF configuration
├── .gitignore                         # Git ignore file
├── main/
│   ├── main.c                         # Main application: 4-channel loadcell reader
│   └── CMakeLists.txt                 # Main component configuration
└── components/ads1261/
    ├── ads1261.h                      # ADS1261 driver API
    ├── ads1261.c                      # ADS1261 driver implementation
    └── CMakeLists.txt                 # Driver component configuration
```

## Quick Start

### 1. Environment Setup
```bash
# Clone and setup ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
source ./export.sh
```

### 2. Build Project
```bash
cd ads1261
idf.py set-target esp32c6
idf.py menuconfig    # Optional: configure if needed
idf.py build
```

### 3. Flash to Device
```bash
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

### 4. Hardware Setup
See [HARDWARE_SETUP.md](HARDWARE_SETUP.md) for:
- Pin connections (SPI: MOSI, MISO, CLK, CS, DRDY)
- Loadcell differential pair configuration
- Signal conditioning and filtering
- External reference setup (ratiometric measurement)
- Calibration procedures

## Key Features
- ✅ 4-channel differential loadcell measurements
- ✅ 24-bit ADC resolution with 128x PGA gain
- ✅ 600 SPS high data rate
- ✅ Ratiometric measurement with external reference
- ✅ SPI communication interface
- ✅ ESP-IDF FreeRTOS application
- ✅ Complete hardware documentation

## Configuration

### ADS1261 Settings
Located in main/main.c:
- `PGA_GAIN = 128x` - High resolution amplification
- `DATA_RATE = 600 SPS` - High speed sampling
- `REFERENCE = External (5V)` - Ratiometric measurement with excitation voltage

### Calibration
Adjust in main/main.c:
- `SCALE_FACTOR` - ADC count to mV conversion
- `ZERO_OFFSET` - Baseline offset from zero load
- `CALIBRATION_FACTOR` - mV to kg conversion factor

## Project Status
- [x] VSCode and ESP-IDF configuration
- [x] Toolchain setup
- [x] ADS1261 sample code provided
- [x] 4-channel loadcell application
- [x] Hardware setup documentation
- [x] Differential measurement configuration
- [x] Ratiometric measurement support
