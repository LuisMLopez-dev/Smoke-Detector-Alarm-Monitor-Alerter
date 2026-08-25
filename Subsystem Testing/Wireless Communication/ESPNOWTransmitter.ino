/*
  TEST: Transmitter; Wireless Communication Type: ESPNOW (Device to Device)

  PURPOSE:
  Verify ESP-NOW data transmission.

  METHOD:
  - Send alternating true/false every 1 second

  EXPECTED RESULT:
  - Receiver toggles state accordingly
*/

#include <WiFi.h>
#include <esp_now.h>

typedef struct{
  bool alarm;
} Message;

Message msg;

uint8_t receiver[] = {0x24,0x6F,0x28,0xAA,0xBB,0xCC};

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_now_init();

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiver, 6);
  esp_now_add_peer(&peer);
}

void loop(){
  msg.alarm = !msg.alarm;
  esp_now_send(receiver, (uint8_t*)&msg, sizeof(msg));

  if(msg.alarm){
    Serial.println("ON");
  }
  else{
    Serial.println("OFF");
  }

  delay(1000);
}