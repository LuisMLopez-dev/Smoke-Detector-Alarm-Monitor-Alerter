/*
  TEST: Transmitter; Wireless Communication Type: ESPNOW (Device to Device)

  PURPOSE:
  Verify ESP-NOW data transmission from one ESP32-S3 to multiple receivers.

  METHOD:
  - Send alternating true/false every 1 second
  - Transmit to MCU 2 and MCU 3
  - Monitor serial output for send status

  EXPECTED RESULT:
  - Both receivers toggle state accordingly
  - Serial monitor shows successful transmission

  NOTES:
  - One-way communication (transmitter → multiple receivers)
*/

#include <WiFi.h>
#include <esp_now.h>

typedef struct{
  bool alarm;
} Message;

Message msg;

// MAC addresses of RECEIVERS
uint8_t receiver1[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0xF8}; // MCU 2
uint8_t receiver2[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0x74}; // MCU 3

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

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onSent);

  // --- Add RECEIVER 1 ---
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, receiver1, 6);
  peer1.channel = 0;
  peer1.encrypt = false;

  if (esp_now_add_peer(&peer1) != ESP_OK){
    Serial.println("Failed to add peer1");
    return;
  }

  // --- Add RECEIVER 2 ---
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, receiver2, 6);
  peer2.channel = 0;
  peer2.encrypt = false;

  if (esp_now_add_peer(&peer2) != ESP_OK){
    Serial.println("Failed to add peer2");
    return;
  }
}

void loop(){
  // Toggle alarm state
  msg.alarm = !msg.alarm;

  // Send to both receivers
  esp_now_send(receiver1, (uint8_t*)&msg, sizeof(msg));
  esp_now_send(receiver2, (uint8_t*)&msg, sizeof(msg));

  // Debug output
  if (msg.alarm == true){
    Serial.println("ON");
  }
  else{
    Serial.println("OFF");
  }

  delay(1000);
}
