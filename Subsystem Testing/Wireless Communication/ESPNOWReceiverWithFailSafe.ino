/*
  TEST: Receiver; Wireless Communication Type: ESPNOW (Device to Device) with failsafe

  PURPOSE:
  Verify ESP-NOW reception and timeout fail-safe behavior.

  METHOD:
  - Receive data via ESP-NOW
  - Control LED through GPIO pin
  - Activate fail-safe if no data is received after timeout

  EXPECTED RESULT:
  - LED follows received signal
  - LED turns ON after timeout if communication is lost
  - LED resumes normal behavior when communication returns

  NOTES:
  - Designed for one-way communication (transmitter → receiver)
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

// Callback when data is received
void onRecv(const esp_now_recv_info*, const uint8_t* data, int len){

  // Validate message size
  if (len != sizeof(msg)){
    return;
  }

  // Copy received data
  memcpy(&msg, data, sizeof(msg));

  // Update state
  alarmState = msg.alarm;

  // Reset timeout timer
  lastRecv = millis();
}

void setup(){
  Serial.begin(115200);

  // Configure LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Set WiFi mode
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register receive callback
  esp_now_register_recv_cb(onRecv);
}

void loop(){

  // Fail-safe: if no message received within timeout, trigger alarm
  if (millis() - lastRecv > timeout){
    alarmState = true;
  }

  // Control LED
  if (alarmState == true){
    digitalWrite(LED_PIN, HIGH);
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }

  // Debug output
  Serial.println(alarmState);

  delay(100);
}
