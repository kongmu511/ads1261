# Hardware Setup Guide

## Pin Configuration

### ESP32C6 to ADS1261 SPI Connection
| Signal | ESP32C6 Pin | ADS1261 Pin | Notes |
|--------|-------------|-------------|-------|
| MOSI   | GPIO 7      | DIN         | Master Out, Slave In |
| MISO   | GPIO 8      | DOUT        | Master In, Slave Out |
| CLK    | GPIO 6      | SCLK        | Serial Clock |
| CS     | GPIO 9      | CS          | Chip Select |
| DRDY   | GPIO 10     | DRDY        | Data Ready (interrupt) |
| GND    | GND         | GND         | Ground |
| VDD    | 3.3V        | VDD         | Power Supply |

## Loadcell Connections

### Differential Measurement Setup (4 Loadcells)
The ADS1261 has 8 analog inputs (AIN0-AIN7) used in differential pairs:

| Loadcell | Positive | Negative | Differential Pair |
|----------|----------|----------|------------------|
| 1        | AIN0     | AIN1     | Pair 0 |
| 2        | AIN2     | AIN3     | Pair 1 |
| 3        | AIN4     | AIN5     | Pair 2 |
| 4        | AIN6     | AIN7     | Pair 3 |

### Loadcell Wiring Pattern
Each loadcell has 4 wires:
- **Red (E+)**: Excitation + → Connect to AIN(positive)
- **Black (E-)**: Excitation - → Connect to AIN(negative)
- **Green (S+)**: Signal + → Connect to AIN(positive) with R/C filter
- **White (S-)**: Signal - → Connect to AIN(negative) with R/C filter

### External Reference Setup (Ratiometric Measurement)
- **Reference Input**: Connect 5V from excitation supply to REF_P via voltage divider or buffer
- **Reference Ground**: Connect to REF_N (GND)
- **Excitation Supply**: Use stable 5V supply with filtering
  - Add 100µF electrolytic + 100nF ceramic caps near supply
  - Add 10µF + 1µF caps near ADS1261 power pins

## Recommended Signal Conditioning

### Input Filtering (for each loadcell)
- Series resistor: 1kΩ to limit slew rate
- Parallel capacitor: 100nF to ground (RC filter)
- Purpose: Reduce noise and prevent aliasing

### Decoupling
- VDD: 100µF + 100nF (bulk + bypass)
- DVDD: 100µF + 100nF
- REF: 10µF + 100nF

## SPI Communication
- **Clock Speed**: 1 MHz (configured in driver)
- **SPI Mode**: 1 (CPOL=0, CPHA=1)
- **Data Format**: 24-bit ADC output (MSB first)
- **Max Rate**: 600 SPS at PGA_GAIN_128

## Calibration Procedure

### Zero Calibration
1. Remove all load from loadcells
2. Run the system for 1 minute to stabilize
3. Record average voltage reading
4. Update `ZERO_OFFSET` in main.c

### Full-Scale Calibration
1. Apply known reference weight (e.g., 10kg per loadcell)
2. Record voltage reading
3. Calculate: `CALIBRATION_FACTOR = Reference_Weight / Voltage`
4. Update in main.c

## Troubleshooting

**Issue**: ADC reads all zeros or constant values
- Check SPI connection, especially MOSI and MISO
- Verify CS and DRDY pins are connected
- Check ADS1261 power supply and reference voltage

**Issue**: Noisy or unstable readings
- Add RC filters on inputs
- Improve power supply decoupling
- Check for grounding issues
- Move SPI wires away from high-frequency signals

**Issue**: DRDY timeout errors
- Verify DRDY pin connection
- Check data rate setting isn't too high
- Ensure ADS1261 is properly configured

## References
- [ADS1261 Datasheet](https://www.ti.com/document-viewer/ads1261/datasheet)
- [TI Precision Lab Examples](https://github.com/TexasInstruments/precision-adc-examples/tree/main/devices/ads1261)
- [ESP32C6 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
