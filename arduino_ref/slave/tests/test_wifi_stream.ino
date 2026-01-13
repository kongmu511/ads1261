/*
 * WiFi UDP Streaming Test - 1000 SPS per channel
 * Uses hardware timer for precise 1000 Hz sampling
 * Streams 4-channel ADC data to PC via UDP
 */

#include "ADS1261.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi AP Configuration
const char* AP_SSID = "ZPlate";                // Access Point name
const char* AP_PASSWORD = "zplate2026";         // AP password (min 8 chars)
const IPAddress AP_IP(192, 168, 4, 1);         // ESP32 IP address
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
const char* PC_IP = "192.168.4.100";           // PC will get this IP
const uint16_t UDP_PORT = 5555;

// Hardware Pins
const int DRDY_PIN = 10;
const int PWDN_PIN = 18;
const int RESET_PIN = 19;
const int START_PIN = 20;
const int SCK = 6;
const int MISO = 2;
const int MOSI = 7;

// Timer Configuration
hw_timer_t* timer = NULL;
volatile bool sampleReady = false;
volatile uint32_t sampleCount = 0;

// Streaming Control
volatile bool streamingEnabled = false;

// ADC
ADS1261 adc;
ADS1261_REGISTERS_Type regMap;

// WiFi & UDP
WiFiUDP udp;

// Data packet structure (20 bytes)
struct __attribute__((packed)) DataPacket {
  uint32_t timestamp_us;  // 4 bytes
  int32_t ch1;           // 4 bytes
  int32_t ch2;           // 4 bytes
  int32_t ch3;           // 4 bytes
  int32_t ch4;           // 4 bytes
};

// Command packet structure (1 byte)
struct __attribute__((packed)) CommandPacket {
  uint8_t cmd;  // 'S' = start, 'E' = end, 'T' = tare
};

// DRDY interrupt
void IRAM_ATTR drdyISR() {
  adc.setDataReady();
}

// Timer interrupt - triggers at 1000 Hz
void IRAM_ATTR onTimer() {
  sampleReady = true;
  sampleCount++;
}

void setupWiFi() {
  Serial.println("\n=== WiFi AP Setup ===");
  Serial.print("Creating Access Point: ");
  Serial.println(AP_SSID);
  
  // Configure AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  
  bool success = WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  if (success) {
    Serial.println("\n✓ WiFi AP started!");
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("\n📱 Connect your PC to 'ZPlate' WiFi network");
    Serial.println("   Then run: python wifi_receiver_gui.py");
    Serial.print("Streaming to: ");
    Serial.print(PC_IP);
    Serial.print(":");
    Serial.println(UDP_PORT);
    
    udp.begin(UDP_PORT);
  } else {
    Serial.println("\n✗ Failed to create AP!");
  }
}

void setupHardware() {
  pinMode(PWDN_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(START_PIN, OUTPUT);
  pinMode(DRDY_PIN, INPUT);
  
  digitalWrite(PWDN_PIN, HIGH);
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(START_PIN, LOW);
  
  adc.begin();
  adc.setDrdyPin(DRDY_PIN);
  adc.attachDrdyInterrupt(drdyISR);
  
  delay(100);
  
  digitalWrite(RESET_PIN, LOW);
  delay(10);
  digitalWrite(RESET_PIN, HIGH);
  delay(100);
}

void configureADC() {
  adc.readAllRegisters(&regMap);
  
  // 40kSPS data rate for fast channel switching
  regMap.MODE0.bit.FILTER = ADS1261_FILTER_SINC1;
  regMap.MODE0.bit.DR = ADS1261_DR_40000_SPS;
  
  // Pulse conversion mode
  regMap.MODE1.bit.DELAY = ADS1261_DELAY_0_US;
  regMap.MODE1.bit.CONVRT = ADS1261_CONVRT_PULSE_CONVERSION;
  
  // PGA = 1 (no gain)
  regMap.MODE2.bit.GAIN = ADS1261_PGA_1;
  
  // Auto SPI timeout
  regMap.MODE3.bit.SPITIM = SPITIM_AUTO_ENABLE;
  
  adc.writeAllRegisters(&regMap);
  delay(100);
}

void setupTimer() {
  // Configure timer for 1000 Hz (1ms period)
  // ESP32-C6: 40 MHz APB clock
  timer = timerBegin(0, 40, true);  // Timer 0, prescaler 40 (40MHz/40 = 1MHz)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);  // 1000 ticks at 1MHz = 1ms = 1000 Hz
  timerAlarmEnable(timer);
  
  Serial.println("✓ Timer configured: 1000 Hz");
}

void checkCommands() {
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    CommandPacket cmd;
    udp.read((uint8_t*)&cmd, sizeof(cmd));
    
    switch (cmd.cmd) {
      case 'S':  // Start streaming
        streamingEnabled = true;
        sampleCount = 0;
        Serial.println("▶ Streaming STARTED");
        break;
        
      case 'E':  // End streaming
        streamingEnabled = false;
        Serial.println("■ Streaming STOPPED");
        break;
        
      case 'T':  // Tare (future implementation)
        Serial.println("⚖ Tare command received");
        break;
    }
  }
}

void setup() {
  Serial.begin(921600);
  delay(1000);
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  WiFi Streaming Test - 1000 SPS      ║");
  Serial.println("║  Timer-based sampling                 ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  setupHardware();
  configureADC();
  setupWiFi();
  setupTimer();
  
  Serial.println("\n✓ Ready! Waiting for START command from PC...");
  Serial.println("Commands: 'S' = Start, 'E' = End, 'T' = Tare");
}

void loop() {
  checkCommands();
  
  if (sampleReady && streamingEnabled) {
    sampleReady = false;
    
    DataPacket packet;
    packet.timestamp_us = micros();
    
    // Read all 4 channels (takes ~885µs with interrupt-driven DRDY)
    packet.ch1 = adc.readChannel(INPMUX_MUXP_AIN2, INPMUX_MUXN_AIN3);
    packet.ch2 = adc.readChannel(INPMUX_MUXP_AIN4, INPMUX_MUXN_AIN5);
    packet.ch3 = adc.readChannel(INPMUX_MUXP_AIN6, INPMUX_MUXN_AIN7);
    packet.ch4 = adc.readChannel(INPMUX_MUXP_AIN8, INPMUX_MUXN_AIN9);
    
    // Send UDP packet
    udp.beginPacket(PC_IP, UDP_PORT);
    udp.write((uint8_t*)&packet, sizeof(packet));
    udp.endPacket();
    
    // Debug output every 1000 samples
    if (sampleCount % 1000 == 0) {
      Serial.print("📊 Samples sent: ");
      Serial.print(sampleCount);
      Serial.print(" | CH1: ");
      Serial.print(packet.ch1);
      Serial.print(" | CH2: ");
      Serial.print(packet.ch2);
      Serial.print(" | CH3: ");
      Serial.print(packet.ch3);
      Serial.print(" | CH4: ");
      Serial.println(packet.ch4);
    }
  }
  if PC is still connected to AP
  static unsigned long lastAPCheck = 0;
  if (millis() - lastAPCheck > 5000) {  // Check every 5 seconds
    int stations = WiFi.softAPgetStationNum();
    if (stations == 0 && streamingEnabled) {
      Serial.println("⚠ No devices connected to ZPlate AP");
    }
    lastAPCheck = millis(nect();
    delay(1000);
  }
}
