/*
  TEST: Transmitter; Wireless Communication Type: ESPNOW (Device to Device)

  PURPOSE:
  Verify ESP-NOW data transmission from one ESP32-S3 to another.

  METHOD:
  - Send alternating true/false every 1 second
  - Monitor serial output for send status

  EXPECTED RESULT:
  - Receiver toggles state accordingly
  - Serial monitor shows successful transmission

  NOTES:
  - One-way communication (no response from receiver required)
*/

#include <WiFi.h>
#include <esp_now.h>

typedef struct{
  bool alarm;
} Message;

Message msg;

// MAC address of RECEIVER (MCU 2)
uint8_t receiver[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0xF8};

// Callback to confirm send status
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status){
  Serial.print("Send Status: ");

  if (status == ESP_NOW_SEND_SUCCESS){
    Serial.println("Success");
  }
  else{
    Serial.println("Fail");
  }
}

void setup(){
  Serial.begin(115200);

  // Set device as WiFi station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // improves ESP-NOW reliability

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register send callback
  esp_now_register_send_cb(onSent);

  // Add receiver as peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiver, 6);
  peer.channel = 0;       // use current WiFi channel
  peer.encrypt = false;   // no encryption

  if (esp_now_add_peer(&peer) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void loop(){
  // Toggle alarm state
  msg.alarm = !msg.alarm;

  // Send message
  esp_now_send(receiver, (uint8_t*)&msg, sizeof(msg));

  // Debug output
  if (msg.alarm == true){
    Serial.println("ON");
  }
  else{
    Serial.println("OFF");
  }

  delay(1000);
}
