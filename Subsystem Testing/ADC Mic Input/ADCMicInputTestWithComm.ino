/*
  TEST: ADC Mic Input Test with Wireless Communication Type: ESPNOW

  PURPOSE:
  Verify if ESP-NOW transmission affects ADC readings, specifically the midpoint stability

  METHOD:
  - Continuously sample ADC at 8 kHz
  - Send ESP-NOW messages every 1 second
  - Print ADC raw and the amplitude

  EXPECTED RESULT:
  - The midpoint of about 1400 remains stable
  - No major distortion during transmission
*/

#include <WiFi.h>
#include <esp_now.h>

#define ADC_PIN 1
#define ADC_MIDPOINT 1400

#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)

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
  analogReadResolution(12); // ADC setup

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onSent);

  // Add Receiver 1 
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, receiver1, 6);
  peer1.channel = 0;
  peer1.encrypt = false;

  if (esp_now_add_peer(&peer1) != ESP_OK){
    Serial.println("Failed to add peer1");
    return;
  }

  // Add Receiver 2
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
  static unsigned long lastSampleTime = 0;
  static unsigned long lastSendTime = 0;

  unsigned long nowMicros = micros();
  unsigned long nowMillis = millis();

  // ADC Sampling at 8 kHz
  if (nowMicros - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = nowMicros;

    int sample = analogRead(ADC_PIN);
    int amplitude = abs(sample - ADC_MIDPOINT);

    Serial.print(sample);
    Serial.print(",");
    Serial.println(amplitude);
  }

  // ESPNOW Transmission every second
  if (nowMillis - lastSendTime >= 1000){
    lastSendTime = nowMillis;

    // Toggle alarm state
    msg.alarm = !msg.alarm;

    esp_now_send(receiver1, (uint8_t*)&msg, sizeof(msg));
    esp_now_send(receiver2, (uint8_t*)&msg, sizeof(msg));

    // Debug output
    if (msg.alarm == true){
      Serial.println("ON");
    }
    else{
      Serial.println("OFF");
    }
  }
}