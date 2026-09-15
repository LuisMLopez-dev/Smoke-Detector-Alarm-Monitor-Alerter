/*
  Integration TEST: T3 Detection with ESP-NOW Transmission and BLE Notification for smartphone app

  PURPOSE:
  Detect Temporal-3 smoke alarm pattern, then:
  - Transmit alarm state to ESP-NOW receivers, the edge and a heartbeat
  - Notify connected phone via BLE with the edge and a heartbeat

  METHOD:
  - Envelope follower with hysteresis and a FSM for T3 detection
  - Alarm latch that is an 8s hold
  - ESP-NOW: edge send on state change and with a 1 Hz heartbeat
  - BLE: advertise as "Safer Signal", notify on same schedule

  EXPECTED RESULT:
  - ESP-NOW receivers mirror alarm state
  - BLE phone receives matching 0/1 notifications
  - No interference between WiFi (ESP-NOW) and BLE stacks
*/

#include <WiFi.h>
#include <esp_now.h>
#include <NimBLEDevice.h>

// Pins and Tuning constants
#define ADC_PIN 1
#define LED_PIN 4

#define ADC_MIDPOINT 1400

#define THRESHOLD_HIGH 400
#define THRESHOLD_LOW 250
#define DECAY 0.95

#define MIN_VALID_STATE_TIME 60 // In ms

#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)

#define PULSE_MIN 200
#define PULSE_MAX 800

#define SHORT_PAUSE_MIN 200
#define SHORT_PAUSE_MAX 800

#define LONG_PAUSE_MIN 800
#define LONG_PAUSE_MAX 2000

#define ALARM_HOLD_TIME 8000

// COMMUNICATION TIMING
#define HEARTBEAT_INTERVAL_MS 1000 // Receiver failsafe timeout is 5s. Heartbeat is at 1s giving a 5x margin.

// BLE CONFIG
#define DEVICE_NAME "Safer Signal"

static NimBLEUUID serviceUUID("12345678-1234-1234-1234-123456789001");
static NimBLEUUID charUUID("12345678-1234-1234-1234-123456789002");

NimBLECharacteristic *alarmCharacteristic;
NimBLEServer *pServer;

bool deviceConnected = false;

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
bool lastAlarmActive = false; // For edge detection

// ESP-NOW
typedef struct{
  bool alarm;
} Message;

Message msg;

unsigned long lastHeartbeat = 0;

uint8_t receiver1[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0xF8}; // MCU 2
uint8_t receiver2[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0x74}; // MCU 3

// BLE SERVER CALLBACKS
class ServerCallbacks : public NimBLEServerCallbacks{
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override{
    deviceConnected = true;
    Serial.println("BLE client connected");
    NimBLEDevice::getAdvertising()->stop();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override{
    deviceConnected = false;
    Serial.printf("BLE client disconnected, reason=%d\n", reason);
    NimBLEDevice::getAdvertising()->start();
    Serial.println("BLE advertising restarted");
  }
};

// ESP-NOW SEND CALLBACK
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status){
  Serial.print("ESP-NOW: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// UNIFIED TRANSMIT: ESP-NOW with BLE
void transmitAlarmState(bool state, bool isEdge){
  // ESP-NOW
  msg.alarm = state;
  esp_now_send(receiver1, (uint8_t *)&msg, sizeof(msg));
  esp_now_send(receiver2, (uint8_t *)&msg, sizeof(msg));

  // BLE
  uint8_t value = state ? 1 : 0;
  alarmCharacteristic->setValue(&value, 1);
  if(deviceConnected){
    alarmCharacteristic->notify();
  }

  // Log of transmission and of heartbeat
  if(isEdge){
    Serial.print("TX Alarm (edge): ");
    Serial.println(state ? "ON" : "OFF"); // Ternary for the logic of the state
  } 
  else{
    Serial.print("[HB ");
    Serial.print(state ? "ON" : "OFF"); // Ternary for the logic of the state
    Serial.println("]");
  }
}

// SETUP
void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(LED_PIN, OUTPUT);

  // BLE Init
  NimBLEDevice::init(DEVICE_NAME);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService *pService = pServer->createService(serviceUUID);

  alarmCharacteristic = pService->createCharacteristic(
    charUUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  // Start known state of alarm = 0; idle alarm
  uint8_t initialValue = 0;
  alarmCharacteristic->setValue(&initialValue, 1);

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(DEVICE_NAME);
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->enableScanResponse(true);
  pAdvertising->start();
  Serial.println("BLE advertising started");

  // ESP-NOW Init
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if(esp_now_init() != ESP_OK){
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, receiver1, 6);
  peer1.channel = 0;
  peer1.encrypt = false;
  if(esp_now_add_peer(&peer1) != ESP_OK) {
    Serial.println("Failed to add peer1");
    return;
  }

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

  // T3 Detection block with ADC sampling
  if(nowMicros - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = nowMicros;

    int sample = analogRead(ADC_PIN);
    int amplitude = abs(sample - ADC_MIDPOINT);

    envelope = envelope * DECAY + amplitude * (1 - DECAY);

    if(currentState){
      if(envelope < THRESHOLD_LOW) 
        currentState = false;
    } 
    else{
      if(envelope > THRESHOLD_HIGH) 
        currentState = true;
    }

    unsigned long now = millis();

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
          if(pulseCount > 0 && pulseCount < 3){
            if(duration < SHORT_PAUSE_MIN || duration > SHORT_PAUSE_MAX){
              pulseCount = 0;
            }
          }
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

    // State timeout protection
    if(currentState){
      if(now - stateStartTime > PULSE_MAX + 200) 
        pulseCount = 0;
    } 
    else{
      if(pulseCount > 0 && pulseCount < 3){
        if(now - stateStartTime > SHORT_PAUSE_MAX + 150) 
          pulseCount = 0;
      } 
      else if(pulseCount == 3){
        if(now - stateStartTime > LONG_PAUSE_MAX + 200) 
          pulseCount = 0;
      }
    }

    // Alarm hold logic
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
  bool edgeChanged  = (alarmActive != lastAlarmActive);
  bool heartbeatDue = (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS);

  if(edgeChanged){
    transmitAlarmState(alarmActive, true);
    lastAlarmActive = alarmActive;
    lastHeartbeat = now;
  } 
  else if(heartbeatDue){
    transmitAlarmState(alarmActive, false);
    lastHeartbeat = now;
  }
}
