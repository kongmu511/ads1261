/**
 * @file test_tare.ino
 * @brief Test 2: Verify tare (zero offset) functionality
 * 
 * Goal: Validate calibration and offset correction
 * Expected: After tare, values near zero; after weight, proportional reading
 */

#include "ads1261.h"
#include <SPI.h>

// Control pins
const int PWDN_PIN = 18;
const int RESET_PIN = 19;
const int START_PIN = 20;

// Calibration factor for DYX-301 100kg load cell
const float CALIB_FACTOR = 2231.19f;

ADS1261 adc;
ADS1261_REGISTERS_Type regMap;

void setupHardware() {
  pinMode(PWDN_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(START_PIN, INPUT);
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(PWDN_PIN, HIGH);
  digitalWrite(START_PIN, LOW);
  
  adc.begin();
  adc.sendCommand(ADS1261_COMMAND_RESET);
  delay(100);
  
  // Configure for stable readings
  adc.readAllRegisters(&regMap);
  regMap.MODE0.bit.DR = ADS1261_DR_2400_SPS;  // Slower for stability
  regMap.MODE1.bit.DELAY = ADS1261_DELAY_50_US;
  regMap.MODE1.bit.CONVRT = ADS1261_CONVRT_CONTINUOUS_CONVERSION;
  regMap.MODE3.bit.SPITIM = SPITIM_AUTO_ENABLE;
  
  adc.writeConfigRegister(ADS1261_MODE0, regMap.MODE0.reg);
  adc.writeConfigRegister(ADS1261_MODE1, regMap.MODE1.reg);
  adc.writeConfigRegister(ADS1261_MODE3, regMap.MODE3.reg);
  delay(100);
}

struct ChannelStats {
  float min;
  float max;
  float avg;
  float stdDev;
};

ChannelStats measureChannel(int samples = 100) {
  float sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
  float min1 = 999999, min2 = 999999, min3 = 999999, min4 = 999999;
  float max1 = -999999, max2 = -999999, max3 = -999999, max4 = -999999;
  
  Serial.print("Collecting ");
  Serial.print(samples);
  Serial.print(" samples");
  
  for (int i = 0; i < samples; i++) {
    if (i % 10 == 0) Serial.print(".");
    
    ChannelData raw = adc.readFourChannel();
    float scale = 1.0f / CALIB_FACTOR;
    float ch1 = raw.ch1 * scale;
    float ch2 = raw.ch2 * scale;
    float ch3 = raw.ch3 * scale;
    float ch4 = raw.ch4 * scale;
    
    sum1 += ch1; sum2 += ch2; sum3 += ch3; sum4 += ch4;
    if (ch1 < min1) min1 = ch1; if (ch1 > max1) max1 = ch1;
    if (ch2 < min2) min2 = ch2; if (ch2 > max2) max2 = ch2;
    if (ch3 < min3) min3 = ch3; if (ch3 > max3) max3 = ch3;
    if (ch4 < min4) min4 = ch4; if (ch4 > max4) max4 = ch4;
    
    delay(5);
  }
  Serial.println(" Done");
  
  Serial.println("\n┌────────┬──────────┬──────────┬──────────┐");
  Serial.println("│ Channel│    Min   │    Max   │   Avg    │");
  Serial.println("├────────┼──────────┼──────────┼──────────┤");
  
  Serial.printf("│   CH1  │ %8.2f │ %8.2f │ %8.2f │\n", min1, max1, sum1/samples);
  Serial.printf("│   CH2  │ %8.2f │ %8.2f │ %8.2f │\n", min2, max2, sum2/samples);
  Serial.printf("│   CH3  │ %8.2f │ %8.2f │ %8.2f │\n", min3, max3, sum3/samples);
  Serial.printf("│   CH4  │ %8.2f │ %8.2f │ %8.2f │\n", min4, max4, sum4/samples);
  Serial.println("└────────┴──────────┴──────────┴──────────┘");
  
  float total = (sum1 + sum2 + sum3 + sum4) / samples;
  Serial.print("Total average: ");
  Serial.print(total, 2);
  Serial.println(" g\n");
  
  ChannelStats stats;
  stats.min = (min1 + min2 + min3 + min4) / 4;
  stats.max = (max1 + max2 + max3 + max4) / 4;
  stats.avg = total;
  return stats;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  ADS1261 Tare/Offset Test            ║");
  Serial.println("║  Goal: Verify calibration works      ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  setupHardware();
  Serial.println("Hardware initialized\n");
  
  // Test 1: Raw readings before tare
  Serial.println("═══ STEP 1: Raw Readings (No Tare) ═══");
  Serial.println("Reading sensors with no calibration...\n");
  adc.set_scale(CALIB_FACTOR);
  ChannelStats before = measureChannel(100);
  
  delay(2000);
  
  // Test 2: After tare
  Serial.println("\n═══ STEP 2: Performing Tare ═══");
  Serial.println("Zeroing all channels...");
  adc.tare();
  Serial.println("Tare complete!\n");
  
  delay(1000);
  
  Serial.println("═══ STEP 3: Readings After Tare ═══");
  Serial.println("Values should be near zero...\n");
  ChannelStats after = measureChannel(100);
  
  // Validation
  Serial.println("\n═══ VALIDATION ═══");
  Serial.print("Average before tare: ");
  Serial.print(before.avg, 2);
  Serial.println(" g");
  
  Serial.print("Average after tare:  ");
  Serial.print(after.avg, 2);
  Serial.println(" g");
  
  Serial.print("Offset applied:      ");
  Serial.print(adc.get_offset(), 0);
  Serial.println(" (raw units)\n");
  
  if (fabs(after.avg) < 10.0f) {
    Serial.println("✓ PASS: Tare working (avg < 10g)");
  } else {
    Serial.println("✗ FAIL: Tare not effective (avg > 10g)");
  }
  
  // Interactive test
  Serial.println("\n═══ STEP 4: Interactive Weight Test ═══");
  Serial.println("Place known weight on sensors and observe...\n");
  Serial.println("Format: [CH1, CH2, CH3, CH4] = Total");
  Serial.println("Press Ctrl+C to stop\n");
}

void loop() {
  ChannelData raw = adc.readFourChannel();
  float scale = 1.0f / CALIB_FACTOR;
  float ch1 = raw.ch1 * scale;
  float ch2 = raw.ch2 * scale;
  float ch3 = raw.ch3 * scale;
  float ch4 = raw.ch4 * scale;
  float total = ch1 + ch2 + ch3 + ch4;
  
  Serial.printf("[%6.1f, %6.1f, %6.1f, %6.1f] = %7.1f g\r", 
                ch1, ch2, ch3, ch4, total);
  
  delay(200);  // Update 5 times per second
}
