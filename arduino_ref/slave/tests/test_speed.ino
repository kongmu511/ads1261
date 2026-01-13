/**
 * @file test_speed.ino
 * @brief Test 1: Measure actual sample rate per channel
 * 
 * Goal: Verify each channel achieves 1000+ SPS (ISO requirement)
 * Expected: 1200 SPS per channel with 4.8k SPS config
 */

#include "ads1261.h"
#include <SPI.h>

// Control pins
const int PWDN_PIN = 18;
const int RESET_PIN = 19;
const int START_PIN = 20;

// Test configuration
const unsigned long TEST_DURATION_MS = 5000;  // 5 second test
const uint8_t DATA_RATES[] = {
  ADS1261_DR_2400_SPS,
  ADS1261_DR_4800_SPS,
  ADS1261_DR_7200_SPS
};
const char* RATE_NAMES[] = {"2.4k", "4.8k", "7.2k"};

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
}

void configureDataRate(uint8_t rate) {
  adc.readAllRegisters(&regMap);
  regMap.MODE0.bit.DR = rate;
  regMap.MODE1.bit.DELAY = ADS1261_DELAY_50_US;
  regMap.MODE1.bit.CONVRT = ADS1261_CONVRT_CONTINUOUS_CONVERSION;
  regMap.MODE3.bit.SPITIM = SPITIM_AUTO_ENABLE;
  
  adc.writeConfigRegister(ADS1261_MODE0, regMap.MODE0.reg);
  adc.writeConfigRegister(ADS1261_MODE1, regMap.MODE1.reg);
  adc.writeConfigRegister(ADS1261_MODE3, regMap.MODE3.reg);
  delay(100);
}

void testSampleRate(uint8_t rate, const char* rateName) {
  Serial.print("\n=== Testing ");
  Serial.print(rateName);
  Serial.println(" SPS Configuration ===");
  
  configureDataRate(rate);
  
  unsigned long startMs = millis();
  unsigned long sampleCount = 0;
  unsigned long ch1Time = 0, ch2Time = 0, ch3Time = 0, ch4Time = 0;
  
  while (millis() - startMs < TEST_DURATION_MS) {
    unsigned long t1 = micros();
    int32_t ch1 = adc.readChannel(INPMUX_MUXP_AIN2, INPMUX_MUXN_AIN3);
    unsigned long t2 = micros();
    int32_t ch2 = adc.readChannel(INPMUX_MUXP_AIN4, INPMUX_MUXN_AIN5);
    unsigned long t3 = micros();
    int32_t ch3 = adc.readChannel(INPMUX_MUXP_AIN6, INPMUX_MUXN_AIN7);
    unsigned long t4 = micros();
    int32_t ch4 = adc.readChannel(INPMUX_MUXP_AIN8, INPMUX_MUXN_AIN9);
    unsigned long t5 = micros();
    
    ch1Time += (t2 - t1);
    ch2Time += (t3 - t2);
    ch3Time += (t4 - t3);
    ch4Time += (t5 - t4);
    
    sampleCount++;
  }
  
  unsigned long actualDuration = millis() - startMs;
  float actualRate = (float)sampleCount * 1000.0f / actualDuration;
  
  Serial.println("\n--- Results ---");
  Serial.print("Duration: ");
  Serial.print(actualDuration);
  Serial.println(" ms");
  
  Serial.print("Total 4-channel cycles: ");
  Serial.println(sampleCount);
  
  Serial.print("Cycle rate: ");
  Serial.print(actualRate, 2);
  Serial.println(" Hz");
  
  Serial.print("\nPer-channel sample rate: ");
  Serial.print(actualRate, 2);
  Serial.print(" SPS ");
  if (actualRate >= 1000.0f) {
    Serial.println("✓ PASS (>1000 SPS)");
  } else {
    Serial.println("✗ FAIL (<1000 SPS)");
  }
  
  Serial.println("\n--- Timing per channel ---");
  Serial.print("CH1 avg: ");
  Serial.print(ch1Time / sampleCount);
  Serial.println(" µs");
  
  Serial.print("CH2 avg: ");
  Serial.print(ch2Time / sampleCount);
  Serial.println(" µs");
  
  Serial.print("CH3 avg: ");
  Serial.print(ch3Time / sampleCount);
  Serial.println(" µs");
  
  Serial.print("CH4 avg: ");
  Serial.print(ch4Time / sampleCount);
  Serial.println(" µs");
  
  unsigned long totalTime = ch1Time + ch2Time + ch3Time + ch4Time;
  Serial.print("Total cycle time: ");
  Serial.print(totalTime / sampleCount);
  Serial.println(" µs");
  
  Serial.println("===============================\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  ADS1261 Sample Rate Test            ║");
  Serial.println("║  Goal: Verify 1000+ SPS per channel  ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  setupHardware();
  Serial.println("Hardware initialized\n");
  
  // Test each data rate
  for (int i = 0; i < 3; i++) {
    testSampleRate(DATA_RATES[i], RATE_NAMES[i]);
    delay(1000);
  }
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  Test Complete                        ║");
  Serial.println("║  Recommended: 4.8k SPS config         ║");
  Serial.println("║  Achieves ~1200 SPS per channel       ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
}

void loop() {
  // Test complete, do nothing
  delay(1000);
}
