/**
 * Minimal test: Can ESP32 receive UDP packets?
 * This strips out all ADC code to isolate network issues
 */

#include <WiFi.h>
#include <WiFiUdp.h>

const char* AP_SSID = "ZPlate";
const char* AP_PASSWORD = "zplate2026";
const uint16_t UDP_PORT = 5555;

WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== UDP Receive Test ===");
  
  // Create AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("AP: %s at %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  
  // Start UDP
  udp.begin(UDP_PORT);
  Serial.printf("Listening on port %d\n", UDP_PORT);
  Serial.println("Send any UDP packet to test...\n");
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    Serial.printf("✓ Received %d bytes from %s:%d\n",
                  packetSize,
                  udp.remoteIP().toString().c_str(),
                  udp.remotePort());
    
    // Read packet
    uint8_t buffer[256];
    int len = udp.read(buffer, sizeof(buffer));
    
    Serial.print("Data: ");
    for (int i = 0; i < len; i++) {
      Serial.printf("%02X ", buffer[i]);
      if (buffer[i] >= 32 && buffer[i] < 127) {
        Serial.printf("('%c') ", buffer[i]);
      }
    }
    Serial.println();
    
    // Echo back
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(buffer, len);
    udp.endPacket();
    Serial.println("  → Echoed back\n");
  }
  
  delay(10);  // Small delay to prevent watchdog issues
}
