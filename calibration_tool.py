#!/usr/bin/env python3
"""
GRF Force Platform - Calibration Assistant Tool

This tool helps with the calibration procedure by:
1. Collecting raw ADC readings over a period
2. Computing statistics (mean, std dev)
3. Suggesting calibration constants
4. Generating C code snippets for main.c
"""

import sys
import time
import statistics
from collections import defaultdict

def parse_serial_output(lines, num_channels=4):
    """
    Parse ESP32 serial output to extract ADC values.
    
    Expected format from main.c logging:
    [Frame 123] Force readings (N):
      Ch1: 0.12 N (raw=7f1234, norm=0.0047)
      Ch2: -0.05 N (raw=7f0999, norm=-0.0032)
      ...
    """
    channel_data = defaultdict(list)
    
    for line in lines:
        # Look for raw ADC values in format: raw=XXXXXX
        if "raw=" in line and "norm=" in line:
            try:
                # Extract channel number and raw value
                parts = line.split("Ch")
                if len(parts) > 1:
                    ch_part = parts[1].split(":")[0].strip()
                    ch_num = int(ch_part)
                    
                    raw_part = line.split("raw=")[1].split(",")[0].strip()
                    raw_val = int(raw_part, 16)  # Convert hex to decimal
                    
                    norm_part = line.split("norm=")[1].split(")")[0].strip()
                    norm_val = float(norm_part)
                    
                    channel_data[ch_num].append({
                        'raw': raw_val,
                        'normalized': norm_val
                    })
            except (ValueError, IndexError):
                pass
    
    return channel_data

def compute_statistics(data_points):
    """Compute mean and standard deviation for a list of values."""
    if not data_points:
        return None, None
    mean = statistics.mean(data_points)
    stdev = statistics.stdev(data_points) if len(data_points) > 1 else 0
    return mean, stdev

def generate_calibration_report(zero_data, scale_data, reference_force_n):
    """
    Generate calibration constants from zero and full-scale measurements.
    
    Args:
        zero_data: Channel data with zero load applied
        scale_data: Channel data with reference force applied
        reference_force_n: Known reference force in Newtons
    """
    
    print("\n" + "="*70)
    print("GRF FORCE PLATFORM - CALIBRATION REPORT")
    print("="*70)
    print(f"\nReference Force Applied: {reference_force_n:.2f} N")
    print(f"Calibration Date: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    
    print("\n" + "-"*70)
    print("ZERO CALIBRATION (No Load)")
    print("-"*70)
    
    zero_offsets = {}
    for ch in sorted(zero_data.keys()):
        raw_values = [d['raw'] for d in zero_data[ch]]
        norm_values = [d['normalized'] for d in zero_data[ch]]
        
        mean_raw, std_raw = compute_statistics(raw_values)
        mean_norm, std_norm = compute_statistics(norm_values)
        
        zero_offsets[ch] = mean_raw
        
        print(f"\nChannel {ch}:")
        print(f"  Raw ADC:     {mean_raw:.0f} ± {std_raw:.1f} LSB")
        print(f"  Normalized:  {mean_norm:.6f} ± {std_norm:.6f}")
        print(f"  Samples:     {len(raw_values)}")
    
    print("\n" + "-"*70)
    print("FULL-SCALE CALIBRATION (Known Load Applied)")
    print("-"*70)
    
    sensitivities = {}
    for ch in sorted(scale_data.keys()):
        raw_values = [d['raw'] for d in scale_data[ch]]
        norm_values = [d['normalized'] for d in scale_data[ch]]
        
        mean_raw, std_raw = compute_statistics(raw_values)
        mean_norm, std_norm = compute_statistics(norm_values)
        
        # Calculate sensitivity: Force / Normalized ADC value
        if mean_norm != 0:
            sensitivity = reference_force_n / mean_norm
            sensitivities[ch] = sensitivity
        else:
            sensitivity = 0
        
        print(f"\nChannel {ch}:")
        print(f"  Raw ADC:     {mean_raw:.0f} ± {std_raw:.1f} LSB")
        print(f"  Normalized:  {mean_norm:.6f} ± {std_norm:.6f}")
        print(f"  Sensitivity: {sensitivity:.4f} N per unit")
        print(f"  Samples:     {len(raw_values)}")
    
    # Generate C code
    print("\n" + "="*70)
    print("SUGGESTED C CODE (main/main.c)")
    print("="*70)
    print("\n/* Update these calibration constants after measurement */\n")
    
    # Use Channel 1 as representative (or average all channels)
    avg_zero = statistics.mean(zero_offsets.values())
    avg_sensitivity = statistics.mean(sensitivities.values())
    
    print(f"static const float ZERO_OFFSET_RAW = {avg_zero:.1f};  /* Measured zero-load ADC value */")
    print(f"static const float FORCE_SENSITIVITY = {avg_sensitivity:.6f};  /* N per normalized unit */")
    
    print("\n" + "="*70)
    print("NEXT STEPS")
    print("="*70)
    print(f"""
1. Copy the above constants into main/main.c
2. Rebuild and flash: idf.py build && idf.py flash
3. Test with intermediate weights to verify linearity
4. If accuracy is poor:
   - Check loadcell wiring polarity (red/black)
   - Verify reference voltage stability
   - Repeat calibration with fresh reference load
""")
    
    return zero_offsets, sensitivities

def interactive_calibration():
    """Interactive calibration wizard."""
    print("\n" + "="*70)
    print("GRF FORCE PLATFORM - INTERACTIVE CALIBRATION WIZARD")
    print("="*70)
    
    print("""
This tool guides you through the calibration procedure.

PROCEDURE:
1. Zero Calibration: Remove ALL load from loadcells, collect readings
2. Full-Scale Calibration: Apply known reference force, collect readings
3. Generate calibration constants
4. Update main.c with new constants
5. Rebuild and test

REQUIREMENTS:
- ESP32-C6 + ADS1261 hardware assembled and running
- Serial terminal connected (ESP-IDF monitor)
- Reference weight/load with known force value
- ~20-30 seconds of stable measurement data
""")
    
    print("\nSTEP 1: ZERO CALIBRATION")
    print("-" * 70)
    input("1. Remove ALL load from the 4 loadcells")
    input("2. Wait for readings to stabilize")
    input("3. Copy 10-20 lines of 'Force readings' output from serial monitor")
    print("   (Paste below, then press Enter twice to finish):\n")
    
    zero_lines = []
    while True:
        try:
            line = input()
            if not line:
                break
            zero_lines.append(line)
        except EOFError:
            break
    
    zero_data = parse_serial_output(zero_lines)
    
    if not zero_data:
        print("\n❌ ERROR: No valid data found. Check serial output format.")
        return
    
    print(f"✓ Parsed {sum(len(v) for v in zero_data.values())} zero calibration samples")
    
    print("\n" + "="*70)
    print("STEP 2: FULL-SCALE CALIBRATION")
    print("-" * 70)
    
    try:
        ref_force = float(input("Enter the known reference force (in Newtons): "))
    except ValueError:
        print("Invalid input. Using 100 N as default.")
        ref_force = 100.0
    
    input(f"1. Apply {ref_force:.1f} N reference load to loadcells")
    input("2. Ensure weight is distributed evenly across all 4 channels")
    input("3. Wait for readings to stabilize")
    input("4. Copy 10-20 lines of 'Force readings' output from serial monitor")
    print("   (Paste below, then press Enter twice to finish):\n")
    
    scale_lines = []
    while True:
        try:
            line = input()
            if not line:
                break
            scale_lines.append(line)
        except EOFError:
            break
    
    scale_data = parse_serial_output(scale_lines)
    
    if not scale_data:
        print("\n❌ ERROR: No valid data found. Check serial output format.")
        return
    
    print(f"✓ Parsed {sum(len(v) for v in scale_data.values())} full-scale calibration samples")
    
    # Generate report
    zero_offsets, sensitivities = generate_calibration_report(
        zero_data, scale_data, ref_force
    )
    
    print("\n✓ Calibration complete!")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--test":
        # Test mode with sample data
        sample_zero = """
[Frame 100] Force readings (N):
  Ch1: 0.00 N (raw=7f0000, norm=0.000000)
  Ch2: 0.00 N (raw=7f0000, norm=0.000000)
  Ch3: 0.00 N (raw=7f0000, norm=0.000000)
  Ch4: 0.00 N (raw=7f0000, norm=0.000000)
"""
        sample_scale = """
[Frame 200] Force readings (N):
  Ch1: 25.00 N (raw=7f4000, norm=0.031250)
  Ch2: 24.99 N (raw=7f3f80, norm=0.031237)
  Ch3: 25.02 N (raw=7f4020, norm=0.031269)
  Ch4: 24.98 N (raw=7f3f70, norm=0.031225)
"""
        zero_data = parse_serial_output(sample_zero.split('\n'))
        scale_data = parse_serial_output(sample_scale.split('\n'))
        generate_calibration_report(zero_data, scale_data, 100.0)
    else:
        interactive_calibration()
