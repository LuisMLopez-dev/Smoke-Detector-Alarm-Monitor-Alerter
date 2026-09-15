/*
  TEST: T3 Detection with wireless communication: ESP-NOW Transmission

  PURPOSE:
  Detect Temporal-3 smoke alarm pattern, then transmit alarm state
  to multiple receivers via ESP-NOW.

  METHOD:
  - Envelope follower for stable sound detection
  - Hysteresis on envelope
  - FSM timing validation for T3 pattern
  - Alarm latch to reduce false negatives
  - LED indicates alarm state
  - ESP-NOW: edge send (instant on state change) + 1 Hz heartbeat (watchdog)

  EXPECTED RESULT:
  - Stable T3 detection
  - Receivers get "ON" immediately on T3 detection, "OFF" when latch clears
  - Heartbeat keeps receiver failsafe from tripping during normal operation
*/

#include <WiFi.h>
#include <esp_now.h>

// Pins and Tuning constants
#define ADC_PIN 1
#define LED_PIN 4

#define ADC_MIDPOINT 1400

// Envelope and Hysteresis
#define THRESHOLD_HIGH 400
#define THRESHOLD_LOW 250
#define DECAY 0.95

#define MIN_VALID_STATE_TIME 60 // In ms

// Sampling
#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)

// T3 timing in ms
#define PULSE_MIN 200
#define PULSE_MAX 800

#define SHORT_PAUSE_MIN 200
#define SHORT_PAUSE_MAX 800

#define LONG_PAUSE_MIN 800
#define LONG_PAUSE_MAX 2000

#define ALARM_HOLD_TIME 8000

// ESP-NOW TIMING
#define HEARTBEAT_INTERVAL_MS 1000 // Receiver failsafe timeout is 5s. Heartbeat is at 1s giving a 5x margin.

// T3 DETECTION STATE
bool currentState = false;
bool lastState = false;

float envelope = 0;

unsigned long lastSampleTime = 0;
unsigned long lastTransition = 0;
unsigned long alarmLatchedTime = 0;
unsigned long stateStartTime = 0;

int pulseCount = 0;
bool alarmActive = false;
bool lastAlarmActive = false; // For edge detection on ESP-NOW send

// ESP-NOW
typedef struct {
  bool alarm;
} Message;

Message msg;

unsigned long lastHeartbeat = 0;

// MAC addresses of RECEIVERS
uint8_t receiver1[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0xF8}; // MCU 2
uint8_t receiver2[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0x74}; // MCU 3

// Callback to confirm send status
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status){
  // Comment out to reduce serial spam if needed
  Serial.print("Send Status: ");
  if(status == ESP_NOW_SEND_SUCCESS){
    Serial.println("Success");
  } 
  else{
    Serial.println("Fail");
  }
}

// ESP-NOW TRANSMIT HELPER
void transmitAlarmState(bool state, bool isEdge){
  msg.alarm = state;
  esp_now_send(receiver1, (uint8_t *)&msg, sizeof(msg));
  esp_now_send(receiver2, (uint8_t *)&msg, sizeof(msg));

  if(isEdge){
    Serial.print("TX Alarm (edge): ");
    Serial.println(state ? "ON" : "OFF");
  } 
  else{
    Serial.print("[HB ");
    Serial.print(state ? "ON" : "OFF");
    Serial.println("]");
  }
}

// SETUP
void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(LED_PIN, OUTPUT);

  // ESP-NOW Init 
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if(esp_now_init() != ESP_OK){
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onSent);

  // Add RECEIVER 1
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, receiver1, 6);
  peer1.channel = 0;
  peer1.encrypt = false;
  if(esp_now_add_peer(&peer1) != ESP_OK){
    Serial.println("Failed to add peer1");
    return;
  }

  // Add RECEIVER 2
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, receiver2, 6);
  peer2.channel = 0;
  peer2.encrypt = false;
  if(esp_now_add_peer(&peer2) != ESP_OK){
    Serial.println("Failed to add peer2");
    return;
  }

  Serial.println("Transmitter ready.");
}

// LOOP
void loop(){
  unsigned long nowMicros = micros();

  // Sampling at fixed rate
  if(nowMicros - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = nowMicros;

    int sample = analogRead(ADC_PIN);
    int amplitude = abs(sample - ADC_MIDPOINT);

    // Envelope follower
    envelope = envelope * DECAY + amplitude * (1 - DECAY);

    // Hysteresis on envelope
    if(currentState){
      if(envelope < THRESHOLD_LOW){
        currentState = false;
      }
    } 
    else{
      if(envelope > THRESHOLD_HIGH){
        currentState = true;
      }
    }

    unsigned long now = millis();

    // State transition detection
    if(currentState != lastState){
      unsigned long duration = now - lastTransition;

      if(duration >= MIN_VALID_STATE_TIME){
        lastTransition = now;
        stateStartTime = now;

        if(lastState){ // Beep ended
          if(duration >= PULSE_MIN && duration <= PULSE_MAX){
            pulseCount++;
            Serial.println("Pulse OK");
          } 
          else{
            pulseCount = 0;
          }
        } 
        else{ // Pause ended
          // Validate short pauses between beeps
          if(pulseCount > 0 && pulseCount < 3){
            if(duration < SHORT_PAUSE_MIN || duration > SHORT_PAUSE_MAX){
              pulseCount = 0;
            }
          }

          // Validate full T3 pattern
          if(pulseCount == 3){
            if(duration >= LONG_PAUSE_MIN && duration <= LONG_PAUSE_MAX){
              Serial.println("T3 DETECTED");
              alarmActive = true;
              alarmLatchedTime = now;
            }
            pulseCount = 0;
          }
        }

        lastState = currentState;
      }
    }

    // State-based timeout protection
    if(currentState){
      if(now - stateStartTime > PULSE_MAX + 200){
        pulseCount = 0;
      }
    } 
    else{
      if(pulseCount > 0 && pulseCount < 3){
        if(now - stateStartTime > SHORT_PAUSE_MAX + 150){
          pulseCount = 0;
        }
      } 
      else if(pulseCount == 3){
        if (now - stateStartTime > LONG_PAUSE_MAX + 200){
          pulseCount = 0;
        }
      }
    }

    // Alarm Hold Logic
    if(alarmActive){
      if(now - alarmLatchedTime <= ALARM_HOLD_TIME){
        digitalWrite(LED_PIN, HIGH);
      } 
      else{
        alarmActive = false;
        digitalWrite(LED_PIN, LOW);
      }
    } 
    else{
      digitalWrite(LED_PIN, LOW);
    }
  }

  // Unified TX: edge and a heartbeat
  unsigned long now = millis();
  bool edgeChanged = (alarmActive != lastAlarmActive);
  bool heartbeatDue = (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS);

  if(edgeChanged){
    transmitAlarmState(alarmActive, true);
    lastAlarmActive = alarmActive;
    lastHeartbeat = now; // Reset heartbeat timer on edge send
  } 
  else if(heartbeatDue){
    transmitAlarmState(alarmActive, false);
    lastHeartbeat = now;
  }
}
