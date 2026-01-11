Using ESP32C6 and TI's ADS1261 ADC to build a **force platform for measuring ground reaction force** (human biomechanics testing).

## Application: Ground Reaction Force Measurement
- Measure vertical, anterior-posterior, and medial-lateral ground reaction forces
- 4 differential strain gauge loadcells (Wheatstone bridge configuration)
- **Higher data rate required**: 150-600 SPS per channel for accurate GRF capture
- **Ratiometric bridge measurement**: No need to know exact reference voltage value

## Loadcell Design - Ratiometric Bridge Measurement
- Use ADS1261's differential mode for Wheatstone bridge (strain gauge transducers)
- **Ratiometric measurement** with external reference voltage
- **Key advantage**: Bridge output is naturally ratiometric to excitation voltage
  - Variations in reference voltage automatically cancel out
  - Measurement accuracy doesn't depend on knowing exact Vref value
  - Only need to calibrate zero offset (tare) and force sensitivity
- Configure PGA gain to 128 with 600+ SPS data rate for force platform applications
- Reference: [SBAA532 - Bridge Measurements](https://www.ti.com/document-viewer/lit/html/sbaa532)

## System Design Considerations
- **Multiplexing**: 4 channels sequential = 150-250 SPS per channel (at 600-1000 SPS system rate)
- **Conversion Latency**: See [SBAA535 - Conversion Latency](https://www.ti.com/document-viewer/lit/html/sbaa535)
- **Data Rate Optimization**: See [DATA_RATE_ANALYSIS.md](DATA_RATE_ANALYSIS.md)

## References
- **TI ADS1261 Datasheet**: https://www.ti.com/document-viewer/ads1261/datasheet
- **SBAA532 - Basic Guide to Bridge Measurements**: https://www.ti.com/document-viewer/lit/html/sbaa532
  - Wheatstone bridge fundamentals
  - Ratiometric measurement explanation
  - Why reference voltage variation doesn't affect accuracy
- **SBAA535 - Conversion Latency & System Cycle Time**: https://www.ti.com/document-viewer/lit/html/sbaa535
  - Multiplexing analysis for multi-channel systems
  - Settling time calculations
- **TI Precision Lab Examples**: https://github.com/TexasInstruments/precision-adc-examples/tree/main/devices/ads1261
- **TI E2E Forum**: https://e2e.ti.com/support/data-converters-group/data-converters/f/data-converters-forum

## Hardware and Toolchain

* ESP32C6
* TI ADS1261 ADC, cs pin is tie to ground.
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
├── DATA_RATE_ANALYSIS.md              # ADS1261 data rate calculations and optimization
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

### 5. Data Rate Optimization
See [DATA_RATE_ANALYSIS.md](DATA_RATE_ANALYSIS.md) for:
- ADS1261 **maximum 40 kSPS** specifications
- **10 kSPS effective per-channel rate** (4-channel multiplexing)
- Trade-offs: speed vs. precision vs. power consumption
- Recommendations for different use cases

## Key Features
- ✅ 4-channel differential loadcell measurements
- ✅ **Maximum 40 kSPS** (10 kSPS per channel with 4-channel multiplexing)
- ✅ Currently configured for 600 SPS (balanced precision vs. speed)
- ✅ Ratiometric measurement with external reference
- ✅ SPI communication interface
- ✅ ESP-IDF FreeRTOS application
- ✅ Complete hardware documentation
- ✅ Data rate analysis and optimization guide
- ✅ Complete hardware docuBalanced precision and speed
- `REFERENCE = External (5V)` - Ratiometric measurement with excitation voltage
- See [DATA_RATE_ANALYSIS.md](DATA_RATE_ANALYSIS.md) for optimization options

### Data Rate Options
The ADS1261 supports data rates from **2.5 SPS to 40 kSPS**:

| Config | System Rate | Per-Channel (4x) | Use Case |
|--------|------------|------------------|----------|
| Maximum | 40 kSPS | 10 kSPS | Real-time monitoring |
| High | 10 kSPS | 2.5 kSPS | Fast sampling |
| **Current** | **600 SPS** | **150 SPS** | **Balanced (Recommended)** |
| Low Power | 20 SPS | 5 SPS | Long-term logging |
## Configuration

### ADS1261 Settings
Located in main/main.c:
- [x] **Data rate analysis and calculations (40 kSPS max, 10 kSPS per channel)**
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
