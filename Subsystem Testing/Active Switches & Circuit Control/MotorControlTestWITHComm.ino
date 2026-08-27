/*
  TEST: Receiver and Motor Control; ESPNOW + PWM Integration

  PURPOSE:
  Verify that the ESP32-S3 can:
  - Receive ESP-NOW messages
  - Control a vibration motor using PWM
  - Maintain stable operation without blocking communication

  METHOD:
  - Receive alarm signal via ESP-NOW
  - Trigger motor with startup kick and sustain PWM
  - Use millis() instead of delay() for non-blocking timing, as compared to the MotorControlNoComm test sketch
  - Activate fail-safe if communication is lost

  EXPECTED RESULT:
  - Motor turns ON when signal is received
  - Motor turns OFF when signal is false
  - Motor turns ON if communication is lost (fail-safe)
  - No missed packets or resets
*/

#include <WiFi.h>
#include <esp_now.h>

#define MOTOR_PIN 4
#define LED_PIN 5  // Debug LED for testing

// PWM settings
#define PWM_FREQ 5000
#define PWM_RES 8

#define START_DUTY 230
#define RUN_DUTY 200
#define START_TIME 200

unsigned long lastRecv = 0;
const unsigned long timeout = 5000;

// Motor state tracking
bool alarmState = false;
bool motorRunning = false;
unsigned long motorStartTime = 0;

// Message structure
typedef struct{
  bool alarm;
} Message;

Message msg;

// Receive callback
void onRecv(const esp_now_recv_info*, const uint8_t* data, int len){

  if (len != sizeof(msg)){
    return;
  }

  memcpy(&msg, data, sizeof(msg));

  alarmState = msg.alarm;
  lastRecv = millis();
}

void setup(){
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK){
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onRecv);

  // Setup PWM for motor
  ledcAttach(MOTOR_PIN, PWM_FREQ, PWM_RES);
  ledcWrite(MOTOR_PIN, 0);
}

void loop(){

  // Fail-safe
  if (millis() - lastRecv > timeout){
    alarmState = true;
  }

  // Motor Control Logic
  if (alarmState == true){
    // If the motor is off, and it just turned ON based on the alarm state
    if (motorRunning == false){
      motorRunning = true;
      motorStartTime = millis();

      // Startup kick
      ledcWrite(MOTOR_PIN, START_DUTY);
    }

    // After startup, switch to run duty for lower current
    if (millis() - motorStartTime > START_TIME){
      ledcWrite(MOTOR_PIN, RUN_DUTY);
    }

  }
  else{
    // Turn and maintain motor OFF
    motorRunning = false;
    ledcWrite(MOTOR_PIN, 0);
  }

  // Debug LED for testing
  if (alarmState == true){
    digitalWrite(LED_PIN, HIGH);
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }

  Serial.println(alarmState);

  delay(10); // Very small delay
}