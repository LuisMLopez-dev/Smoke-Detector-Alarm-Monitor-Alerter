#include <WiFi.h> // Control of wireless network connections for Wi-Fi enabled microcontrollers
#include <esp_now.h> // Wireless communication protocol
#include <NimBLEDevice.h> // Lightweight alternative to Bluedroid. Uses less flash and RAM

// Pins and Tuning Constants
#define ADC_PIN 1 // GPIO 1, analog input from the microphone module circuit

#define ADC_MIDPOINT 1400 // Midpoint based on measured value from testing. Used to remove the DC bias from the ADC reading

#define THRESHOLD_HIGH 400 // Threshold for the envelope to rise above before a sound is registered. Tuned during testing
#define THRESHOLD_LOW 250 // Threshold for the envelope to fall below before a sound is considered ended. Lower than THRESHOLD_HIGH to create a hysteresis band and prevent chatter
#define DECAY 0.95 // Envelope follower decay coefficient. Higher = slower decay = smoother envelope. Tuned during testing

#define MIN_VALID_STATE_TIME 60 // Minimum time in ms that a state must be held before it is accepted as a real transition. Filters out very short glitches

#define SAMPLE_RATE 8000 // 8 kHz, or 8,000 samples per second. Nyquist = 4 kHz, LPF before ADC is 4.88 kHz, and smoke alarms can be ~520 Hz up to about 3 kHz
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE) // Time in microseconds between samples. 1,000,000 µs / 8000 = 125 µs per sample

#define PULSE_MIN 200 // Minimum pulse duration in ms. Tuned for the tested alarm pattern
#define PULSE_MAX 800 // Maximum pulse duration in ms

#define SHORT_PAUSE_MIN 200 // Minimum short pause duration in ms (between the first and second pulse, and second and third pulse)
#define SHORT_PAUSE_MAX 800 // Maximum short pause duration in ms

#define LONG_PAUSE_MIN 800 // Minimum long pause duration in ms (after the third pulse, before the next T3 group)
#define LONG_PAUSE_MAX 2000 // Maximum long pause duration in ms

/*
 More deatialed complex breakdown of ALARM_HOLD_TIME
 After a T3 detection, the alarm stays latched for this long even if no further T3 cycles are detected. 
 This prevents a single missed cycle (dropped sample, timing glitch, brief audio dropout) from causing a false "all clear" edge transmission. 
 8s is chosen to be longer than the worst-case T3 repeat period (~4s) so normal alarm repetition naturally refreshes the latch before it expires.
*/

#define ALARM_HOLD_TIME 8000 

// Communication Timing
#define HEARTBEAT_INTERVAL_MS 1000 // Receiver fail-safe timeout is 5s. Heartbeat is at 1s, giving a 5x margin. Keeps receivers from false-triggering their fail-safe

// BLE Configuration
#define DEVICE_NAME "Safer Signal" // Name that appears on the phone when scanning for BLE devices

// Custom 128-bit UUIDs for the BLE service and its alarm characteristic
static NimBLEUUID serviceUUID("12345678-1234-1234-1234-123456789001"); // Service UUID. Groups the related BLE data together
static NimBLEUUID charUUID("12345678-1234-1234-1234-123456789002"); // Characteristic UUID. Holds the 0 or 1 alarm value the phone subscribes to

NimBLECharacteristic *alarmCharacteristic; // Pointer to the characteristic that carries the alarm value
NimBLEServer *pServer; // Pointer to the BLE server object

bool deviceConnected = false; // Tracks whether a phone is currently connected over BLE

// T3 Detection State
bool currentState = false; // Current sound state from the envelope follower. True = above threshold = sound, False = below = silence
bool lastState = false; // Previous sound state, used to detect transitions (sound to silence, or silence to sound)

float envelope = 0; // Envelope follower value. Smooths the rectified amplitude over time

unsigned long lastSampleTime = 0; // Timestamp of the last ADC sample, in microseconds
unsigned long lastTransition = 0; // Timestamp of the last accepted state transition, in milliseconds
unsigned long alarmLatchedTime = 0; // Timestamp when the alarm was last latched on, used for the hold timer
unsigned long stateStartTime = 0; // Timestamp when the current state (sound or silence) began, used for the state timeout protection

int pulseCount = 0; // Counter for the number of valid pulses detected in the current T3 sequence
bool alarmActive = false; // Current alarm state. True = alarm is active, False = idle
bool lastAlarmActive = false; // Previous alarm state, used for edge detection so we know when to send an edge update versus a heartbeat

// ESP-NOW
typedef struct{ // Typedef is used here to make an alias for this struct
  bool alarm; // Flag for the smoke detector alarm, for communication
} Message; // Alias name for this struct

Message msg; // Variable to hold the data that is to be sent

unsigned long lastHeartbeat = 0; // Timestamp of the last heartbeat transmission, in milliseconds

// Receiver MAC Addresses. Real addresses discovered during MAC address test sketch on the receivers
uint8_t receiver1[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0xF8}; // MCU 2, the placeable alarm unit
uint8_t receiver2[] = {0xE8, 0x3D, 0xC1, 0xF5, 0x10, 0x74}; // MCU 3, the wearable alarm unit

// BLE Server Callbacks
// Custom class that reacts to BLE connection events
class ServerCallbacks : public NimBLEServerCallbacks{
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override{
    deviceConnected = true; // Trigger the flag so the loop knows a phone is connected
    Serial.println("BLE client connected"); // Debug message for a successful connection
    NimBLEDevice::getAdvertising()->stop(); // Stop advertising since we now have a client connected
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override{
    deviceConnected = false; // Trigger the flag so the loop knows no phone is connected
    Serial.printf("BLE client disconnected, reason=%d\n", reason); // Debug message with the disconnect reason code
    NimBLEDevice::getAdvertising()->start(); // Restart advertising so the phone can reconnect
    Serial.println("BLE advertising restarted"); // Debug message confirming advertising is active again
  }
};

// ESP-NOW Send Callback
// This function makes the microcontroller send data using the onSent function, acting as an interrupt when a transmission completes
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status){
  Serial.print("ESP-NOW: "); // Prefix for the ESP-NOW transmission status
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail"); // Debug message for successful or unsuccessful transmission
}

// Unified Transmit: ESP-NOW and BLE
// Sends the current alarm state over both ESP-NOW (to both receivers) and BLE (to the phone if connected)
// isEdge tells us whether this is a state change (edge) or a heartbeat, for logging purposes
void transmitAlarmState(bool state, bool isEdge){
  // ESP-NOW Transmission
  msg.alarm = state; // Load the current alarm state into the outgoing message
  esp_now_send(receiver1, (uint8_t *)&msg, sizeof(msg)); // Send to receiver 1 (placeable alarm unit)
  esp_now_send(receiver2, (uint8_t *)&msg, sizeof(msg)); // Send to receiver 2 (wearable alarm unit)

  // BLE Transmission
  uint8_t value = state ? 1 : 0; // A 1-byte value to send over BLE: 0 for no alarm, 1 for alarm. Ternary used instead of if/else; either or works
  alarmCharacteristic->setValue(&value, 1); // &value is a pointer to the data, and 1 is the size in bytes
  if(deviceConnected){ // Data can only be notified if the phone is connected
    alarmCharacteristic->notify(); // Sends a notification to the connected phone with the updated value
  }

  // Log of transmission and of heartbeat
  if(isEdge){ // If this transmission was caused by a change in alarm state
    Serial.print("TX Alarm (edge): "); // Prefix for an edge transmission
    Serial.println(state ? "ON" : "OFF"); // Ternary for the logic of the state
  } 
  else{ // If this transmission was a scheduled heartbeat
    Serial.print("[HB "); // Prefix for a heartbeat transmission
    Serial.print(state ? "ON" : "OFF"); // Ternary for the logic of the state
    Serial.println("]"); // Closing bracket for the heartbeat log
  }
}

void setup(){
  Serial.begin(115200); // Baud rate: speed for serial communication
  analogReadResolution(12); // The ESP32-S3 ADC's resolution is 12 bits

  // BLE Initialization
  NimBLEDevice::init(DEVICE_NAME); // Sets the device name that the phone will see when scanning

  pServer = NimBLEDevice::createServer(); // Turns the ESP32 into a BLE Server
  pServer->setCallbacks(new ServerCallbacks()); // Links the custom class to BLE connection events

  NimBLEService *pService = pServer->createService(serviceUUID); // Creates a BLE service, which acts as a container/category for related data

  /*
    Creates the alarm characteristic and attaches it to the service
    READ allows the phone to request the current value, and NOTIFY allows the ESP32 to push updates
    The line below is broken across multiple lines to make it more readable
  */
  alarmCharacteristic = pService->createCharacteristic(
    charUUID, // UUID of the characteristic
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY // Properties: readable and notifiable
  );

  // Start with a known state of alarm = 0, so the phone sees a defined value on first connect
  uint8_t initialValue = 0; // Initial alarm value
  alarmCharacteristic->setValue(&initialValue, 1); // Sets the initial value on the characteristic

  pService->start(); // Makes the service active

  // Begin advertising so phones can discover and connect to us
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(DEVICE_NAME); // Advertised device name
  pAdvertising->addServiceUUID(serviceUUID); // Advertises the service UUID so phones can find it
  pAdvertising->enableScanResponse(true); // Enables scan response so the name and service are visible during a scan
  pAdvertising->start(); // Starts the advertising
  Serial.println("BLE advertising started"); // Debug message confirming BLE is advertising

  // ESP-NOW Initialization
  WiFi.mode(WIFI_STA); // Sets the microcontroller in station mode, allowing it to send or receive data packets
  WiFi.disconnect(); // Disconnects from any saved Wi-Fi network so ESP-NOW is not disturbed

  if(esp_now_init() != ESP_OK){ // Verifies if the ESP-NOW communication protocol was initialized properly
    Serial.println("Error initializing ESP-NOW"); // Error message
    return; // Exits the setup function by returning nothing
  }

  esp_now_register_send_cb(onSent); // Registers the send callback so onSent runs when a transmission completes

  // Add receiver 1 as a peer
  esp_now_peer_info_t peer1 = {}; // Holds the information of the peer
  memcpy(peer1.peer_addr, receiver1, 6); // Copies the MAC address of receiver 1 into memory
  peer1.channel = 0; // Channel 0 is the auto channel, telling the hardware to use the same channel as the receiver
  peer1.encrypt = false; // Encryption is not needed here as the data being sent is not sensitive, and there is no need to overcomplicate transmission for this safety system
  if(esp_now_add_peer(&peer1) != ESP_OK) { // Checks to see if the peer was not added successfully
    Serial.println("Failed to add peer1"); // Debug message for unsuccessful peer addition
    return; // Exits the setup function by returning nothing
  }

  // Add receiver 2 as a peer
  esp_now_peer_info_t peer2 = {}; // Holds the information of the peer
  memcpy(peer2.peer_addr, receiver2, 6); // Copies the MAC address of receiver 2 into memory
  peer2.channel = 0; // Channel 0 is the auto channel, telling the hardware to use the same channel as the receiver
  peer2.encrypt = false; // Encryption is not needed here as the data being sent is not sensitive
  if(esp_now_add_peer(&peer2) != ESP_OK){ // Checks to see if the peer was not added successfully
    Serial.println("Failed to add peer2"); // Debug message for unsuccessful peer addition
    return; // Exits the setup function by returning nothing
  }

  Serial.println("Transmitter ready."); // Debug message confirming setup is complete
}

void loop(){
  unsigned long nowMicros = micros(); // Obtains the current time in microseconds for the sampling check

  // T3 Detection Block with ADC Sampling
  /*
    This if statement checks if one sample period has elapsed, and then allows a new sample
    The micros() function obtains the current time in microseconds
    1,000,000 is in microseconds which is equal to 1 second
    It is divided by the sample rate to obtain the sample period
    Currently: 1,000,000 µs / 8000 samples per second = 125 µs, or 1 sample every 125 µs
  */
  if(nowMicros - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = nowMicros; // Creates a time stamp of the current time for sampling

    int sample = analogRead(ADC_PIN); // From the ADC pin, sample the voltage
    int amplitude = abs(sample - ADC_MIDPOINT); // Removes the DC offset and takes the absolute value to obtain only positive values

    // Envelope follower: smooths the rectified amplitude over time using an exponential moving average
    // New envelope = (old envelope * decay) + (current amplitude * (1 - decay))
    // This makes brief spikes less likely to trigger the state machine and gives a more stable signal
    envelope = envelope * DECAY + amplitude * (1 - DECAY);

    // Hysteresis-based sound detection on the envelope value
    if(currentState){ // If a beep is ongoing at this moment
      if(envelope < THRESHOLD_LOW){ // Currently ON, and only turn OFF if we drop below the lower threshold
        currentState = false; // The current sound is now silence
      }
    } 
    else{ // If there is silence ongoing at this moment
      if(envelope > THRESHOLD_HIGH){ // Currently OFF, and only turn ON if we exceed the upper threshold
        currentState = true; // The current sound is now a beep
      }
    }

    unsigned long now = millis(); // Obtains the current time in milliseconds, used as a reference for state transitions

    // Detects a state transition by comparing if the current sound state differs from the last, indicating it has gone from either on to off or off to on
    if(currentState != lastState){
      unsigned long duration = now - lastTransition; // Given a change in state, determine the duration that the signal was in that state

      if(duration >= MIN_VALID_STATE_TIME){ // Only accept the transition if the previous state was held long enough to be considered real
        lastTransition = now; // Stores the current time that the state changed, to be used in the next loop to calculate the duration
        stateStartTime = now; // Marks the start of the new state for the timeout protection

        if(lastState){ // The last sound state was on, meaning transitioned from on to off. This is a pulse that ended
          if(duration >= PULSE_MIN && duration <= PULSE_MAX){ // Check if the duration that the pulse was on is in the proper timing window
            pulseCount++; // Valid pulse, increment the counter
            Serial.println("Pulse OK"); // Printing for testing verification
          } 
          else{ // The pulse did not meet the duration window and is invalid
            pulseCount = 0; // Reset the pulse count given an invalid pulse. Acts as a reset given the period of the incoming signal, and also reduces false positives
          }
        } 
        else{ // The last sound state was off, meaning transitioned from off to on. This is a pause that ended
          // Short pause time duration check for the first two pauses
          if(pulseCount > 0 && pulseCount < 3){ // If the pulse count is 1 or 2, we are in a short pause between pulses
            if(duration < SHORT_PAUSE_MIN || duration > SHORT_PAUSE_MAX){ // Ensures that the short pause is of the correct length of time
              pulseCount = 0; // The short pause was not valid, indicating a false T3 pattern. Reset the pulse count
            }
            // If it passed the check, this is a valid short pause and no action is needed. Continue the sequence
          }
          // After 3 pulses, we expect the long pause
          if(pulseCount == 3){
            if(duration >= LONG_PAUSE_MIN && duration <= LONG_PAUSE_MAX){ // Ensures the long pause is of the correct length of time
              Serial.println("T3 DETECTED"); // Temporal 3 has been detected
              alarmActive = true; // Activate the alarm
              alarmLatchedTime = now; // Record the detection time for the hold timer
            }
            pulseCount = 0; // Reset after evaluating the full sequence
          }
        }
        lastState = currentState; // Record the current sound state as the previous sound state after all of the checks for the next loop
      }
    }

    // State timeout protection: if a state is held for too long, the pulse sequence is invalid and should be reset
    if(currentState){ // If a sound is currently ongoing
      if(now - stateStartTime > PULSE_MAX + 200) // If the sound has been held longer than the maximum pulse plus margin
        pulseCount = 0; // The pulse is invalid, reset the sequence
    } 
    else{ // If silence is currently ongoing
      if(pulseCount > 0 && pulseCount < 3){ // If we are in a short pause and waiting for the next pulse
        if(now - stateStartTime > SHORT_PAUSE_MAX + 150) // If the pause has been held longer than the maximum short pause plus margin
          pulseCount = 0; // The sequence is invalid, reset it
      } 
      else if(pulseCount == 3){ // If we just completed 3 pulses and are waiting for the long pause to end
        if(now - stateStartTime > LONG_PAUSE_MAX + 200) // If the pause has been held longer than the maximum long pause plus margin
          pulseCount = 0; // The sequence is invalid, reset it
      }
    }

    // Alarm Hold Logic
    if(alarmActive){ // If the alarm is currently active
      if(now - alarmLatchedTime > ALARM_HOLD_TIME){ // If the hold time since the last detection has expired
        alarmActive = false; // Deactivate the alarm
      }
    }
  }

  // Unified TX: edge and heartbeat
  unsigned long now = millis(); // Obtains the current updated time in ms
  bool edgeChanged  = (alarmActive != lastAlarmActive); // True if the alarm state just changed since the last transmission
  bool heartbeatDue = (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS); // True if it is time for the scheduled heartbeat

  if(edgeChanged){ // Edge takes priority over the heartbeat
    transmitAlarmState(alarmActive, true); // Transmit the new state as an edge update
    lastAlarmActive = alarmActive; // Update the tracked previous state
    lastHeartbeat = now; // Reset the heartbeat timer so we don't immediately send a heartbeat after an edge
  } 
  else if(heartbeatDue){ // If no edge occurred but the heartbeat interval has elapsed
    transmitAlarmState(alarmActive, false); // Transmit the current state as a heartbeat
    lastHeartbeat = now; // Update the heartbeat timer
  }
}
