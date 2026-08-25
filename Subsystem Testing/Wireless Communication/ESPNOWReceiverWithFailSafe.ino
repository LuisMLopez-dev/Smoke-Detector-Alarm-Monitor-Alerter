/*
  TEST: Receiver; Wireless Communication Type: ESPNOW (Device to Device) with failsafe

  PURPOSE:
  Verify ESP-NOW reception and timeout fail-safe behavior using an external LED connected to a GPIO pin.

  METHOD:
  - Receive data via ESP-NOW
  - Control LED through GPIO pin
  - Activate fail-safe if no data is received after a certain amount of time

  EXPECTED RESULT:
  - LED follows received signal
  - LED turns ON after timeout if communication is lost
  - LED returns to turning on and off once communication is established again showcasing non-latching failsafe for proper operation
*/

#include <WiFi.h>
#include <esp_now.h>

#define LED_PIN 4

unsigned long lastRecv = 0;
const unsigned long timeout = 5000;

bool alarmState = false;

typedef struct{
  bool alarm;
} Message;

Message msg;

void onRecv(const esp_now_recv_info*, const uint8_t* data, int len){
  memcpy(&msg, data, sizeof(msg));
  alarmState = msg.alarm;
  lastRecv = millis();
}

void setup(){
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure known startup state

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
}

void loop(){
  // Fail-safe: Trigger alarm if communication is lost
  if (millis() - lastRecv > timeout){
    alarmState = true;
  }

  digitalWrite(LED_PIN, alarmState);
  Serial.println(alarmState);
}
