/**
 * @file test_ble.ino
 * @brief Test 3: Verify BLE connectivity and data transmission
 * 
 * Goal: Test BLE connection, commands, and data streaming
 * Expected: Mobile app can connect, send commands, receive force data
 */

#include "ads1261.h"
#include "ota.h"
#include <SPI.h>

// Control pins
const int PWDN_PIN = 18;
const int RESET_PIN = 19;
const int START_PIN = 20;

// Test state
bool isStreaming = false;
unsigned long streamCount = 0;
unsigned long lastStatsMs = 0;

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
  
  // Configure for 1000 SPS target
  adc.readAllRegisters(&regMap);
  regMap.MODE0.bit.DR = ADS1261_DR_4800_SPS;
  regMap.MODE1.bit.DELAY = ADS1261_DELAY_50_US;
  regMap.MODE1.bit.CONVRT = ADS1261_CONVRT_CONTINUOUS_CONVERSION;
  regMap.MODE3.bit.SPITIM = SPITIM_AUTO_ENABLE;
  
  adc.writeConfigRegister(ADS1261_MODE0, regMap.MODE0.reg);
  adc.writeConfigRegister(ADS1261_MODE1, regMap.MODE1.reg);
  adc.writeConfigRegister(ADS1261_MODE3, regMap.MODE3.reg);
  delay(100);
  
  adc.set_scale(FACTOR);
  adc.tare();
}

void processTestCommand(uint8_t* data, size_t length) {
  if (length < 1) return;
  
  uint8_t cmd = data[0];
  
  switch (cmd) {
    case 0x01:  // START
      isStreaming = true;
      streamCount = 0;
      lastStatsMs = millis();
      Serial.println("✓ START command - streaming enabled");
      break;
      
    case 0x02:  // STOP
      isStreaming = false;
      Serial.println("✓ STOP command - streaming disabled");
      Serial.print("Total packets sent: ");
      Serial.println(streamCount);
      break;
      
    case 0x03:  // TARE
      adc.tare();
      Serial.println("✓ TARE command - sensors zeroed");
      break;
      
    case 0x04:  // SET_CALIB
      if (length >= 5) {
        float factor;
        memcpy(&factor, &data[1], 4);
        adc.set_scale(factor);
        Serial.print("✓ CALIB command - factor set to ");
        Serial.println(factor);
      }
      break;
      
    case 0x05:  // GET_STATUS
      Serial.println("✓ STATUS request");
      Serial.print("  Streaming: ");
      Serial.println(isStreaming ? "YES" : "NO");
      Serial.print("  Packets: ");
      Serial.println(streamCount);
      Serial.print("  Connected: ");
      Serial.println(deviceConnected ? "YES" : "NO");
      break;
      
    default:
      Serial.print("✗ Unknown command: 0x");
      Serial.println(cmd, HEX);
      break;
  }
}

// Override BLE callback for testing
class TestBleCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    uint8_t* pData = pCharacteristic->getData();
    
    if (pData != NULL && value.length() > 0) {
      Serial.print("\nReceived command: 0x");
      Serial.print(pData[0], HEX);
      Serial.print(" (");
      Serial.print(value.length());
      Serial.println(" bytes)");
      
      processTestCommand(pData, value.length());
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  ADS1261 BLE Connection Test         ║");
  Serial.println("║  Goal: Verify BLE commands & data    ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  setupHardware();
  Serial.println("Hardware initialized");
  
  // Initialize BLE with test callback
  Serial.println("Initializing BLE...");
  initBLE();
  
  // Replace callback with test version
  pCharacteristicRX->setCallbacks(new TestBleCallbacks());
  
  Serial.println("BLE ready!");
  Serial.println("\n═══ Test Instructions ═══");
  Serial.println("1. Connect mobile app to 'Slave' device");
  Serial.println("2. Send commands:");
  Serial.println("   0x01 - START streaming");
  Serial.println("   0x02 - STOP streaming");
  Serial.println("   0x03 - TARE (zero)");
  Serial.println("   0x05 - GET STATUS");
  Serial.println("\n3. Observe data packets in app");
  Serial.println("   Format: 0xAA + 4x float32 (17 bytes)");
  Serial.println("\nWaiting for connection...\n");
}

void loop() {
  // Connection status monitoring
  static bool wasConnected = false;
  if (deviceConnected != wasConnected) {
    wasConnected = deviceConnected;
    if (deviceConnected) {
      Serial.println("\n✓ BLE CONNECTED");
      Serial.println("Ready to receive commands\n");
    } else {
      Serial.println("\n✗ BLE DISCONNECTED");
      isStreaming = false;
    }
  }
  
  // Stream data if enabled and connected
  if (isStreaming && deviceConnected) {
    ChannelData raw = adc.readFourChannel();
    float scale = 1.0f / FACTOR;
    float ch1 = raw.ch1 * scale;
    float ch2 = raw.ch2 * scale;
    float ch3 = raw.ch3 * scale;
    float ch4 = raw.ch4 * scale;
    
    // Build packet: 0xAA header + 4 floats
    uint8_t buffer[17];
    buffer[0] = 0xAA;
    memcpy(&buffer[1], &ch1, 4);
    memcpy(&buffer[5], &ch2, 4);
    memcpy(&buffer[9], &ch3, 4);
    memcpy(&buffer[13], &ch4, 4);
    
    pCharacteristicTX->setValue(buffer, 17);
    pCharacteristicTX->notify();
    
    streamCount++;
    
    // Stats every second
    if (millis() - lastStatsMs >= 1000) {
      unsigned long elapsed = millis() - lastStatsMs;
      float rate = streamCount * 1000.0f / elapsed;
      
      Serial.print("Streaming: ");
      Serial.print(streamCount);
      Serial.print(" packets, ");
      Serial.print(rate, 1);
      Serial.print(" Hz | [");
      Serial.print(ch1, 1);
      Serial.print(", ");
      Serial.print(ch2, 1);
      Serial.print(", ");
      Serial.print(ch3, 1);
      Serial.print(", ");
      Serial.print(ch4, 1);
      Serial.println("]");
      
      lastStatsMs = millis();
      streamCount = 0;
    }
    
    delay(1);  // ~1000 Hz rate
  } else {
    delay(100);  // Idle
  }
}
