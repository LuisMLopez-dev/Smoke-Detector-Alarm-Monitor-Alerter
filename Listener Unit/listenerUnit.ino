#include <WiFi.h> // Control of wirelesss network connections for Wi-Fi enabled microcontrollers
#include <esp_now.h> // Wireless communcation protocol

// BLE Library
#include <NimBLEDevice.h> // Lightweird alternative to Bluedroid. Uses less flash and RAM

#define ADC_PIN 1 // GPIO 1
#define SAMPLE_RATE 8000 // 8 kHz, or 8,000 samples a second. This means the Nyquist frequency = 4 kHz, and the LPF before the ADC pin is set to 4.98 kHz, and smoke alarms can be 520 Hz, or about 3 kHz
#define THRESHOLD 200 // Threshold to determine what is sound or silence. Testing is requried to fine tune it

// Timing Windows, the pulse is about 0.5 seconds, the pause is about 1.5 seconds
#define PULSE_MIN 300 // 0.3 seconds or 300 ms
#define PULSE_MAX 700 // 0.7 seconds or 700 ms
#define PAUSE_MIN 1200 // 1.2 seconds or 1200 ms
#define PAUSE_MAX 2000 // 2.0 seconds or 2000 ms

#define ALARM_HOLD_TIME 4000 // 4.0 seconds or 4000 ms to keep alarm active without new detection
#define SEND_INTERVAL 200 // 0.2 seconds or 200 ms between transmissions

typedef struct{ //Typedef is used here to make an alias for this struct
  bool smokeDetectorAlarmTriggered; // Flag for the smoke detector alarm, for communication
} structMessage; // Alias name for this struct

structMessage outgoingData; // Variable to hold the data that is to be sent

// Receiver MAC Addresses
uint8_t receiver1[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC}; // NOTE: Fake address used for compiling. Must be updated after running MAC address test sketch on receivers
uint8_t receiver2[] = {0x24, 0x6F, 0x28, 0x11, 0x22, 0x33}; // NOTE: Fake address used for compiling. Must be updated after running the MAC address test sketch on receivers

// Global STATE VARIABLES
bool currentSoundState = false;
bool lastSoundState = false;

// Global Timing Variables
unsigned long lastStateTransitionTime = 0; // The time when the state changed from either on to off, or off to on
unsigned long lastSampleTime = 0; // Last time in which the analog signal was sampled through the ADC pin
unsigned long lastSendTime = 0; // Last previous time when a message was transmitted
unsigned long lastT3Time = 0; // Last previous time where a T3 code was detected

int pulseCount = 0; // Counter for the number of pulses
bool alarmActive = false; // Default state of the alarm is false

// Global BLE Variables
NimBLECharacteristic *alarmCharacteristic; // Holds the alarm value, 0 or 1
bool deviceConnected = false; // Tracks the connection state to the phone

// Custom class that reacts to BLE events
class MyServerCallbacks: public NimBLEServerCallbacks{
  void onConnect(NimBLEServer* pServer){ // When the phone connects
    deviceConnected = true; // Trigger flag
  }

  void onDisconnect(NimBLEServer* pServer){  // When the phone disconnects
    deviceConnected = false; // Trigger flag
  }
};

// The send callback function ensures that outgoing data was sent 
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status){
  if(status == ESP_NOW_SEND_SUCCESS){ // If the data was successfully sent
    Serial.println("Sent OK"); // Debug message for successful transmission
  } 
  else{ // If the data was not successfully sent
    Serial.println("Send Fail"); // Debug message for unsuccessful transmission
  }
}

// A peer is another ESP32 MCU that is acting as a reciever through ESP-NOW, in which this MCU can communicate with
void addPeer(uint8_t *address){
  esp_now_peer_info_t peerInfo = {}; // Holds the information of the peer
  memcpy(peerInfo.peer_addr, address, 6); // Makes a copy of it in memory
  peerInfo.channel = 0; // Sets it to channel 0, which is the auto channel that tells the hardware to automatically use the same channel as the receiver
  peerInfo.encrypt = false; // Encryption is not needed here as the data being sent is not sensitive and there is no need to overcomplicate transmission for this safety system

  if(esp_now_add_peer(&peerInfo) != ESP_OK){ // Checks to see if the peer was not added successfully
    Serial.println("Failed to add peer"); // Debug message for unsuccessful peer addition
  }
}

void setup(){
  Serial.begin(115200); // Baud rate: speed for serial communication

  analogReadResolution(12); // The ESP32-S3 ADC's resolution is 12 bits
  lastStateTransitionTime = millis(); // Initial current time in ms

  // Sets the microcontroller in station mode, allowing to to send or recieve data packets
  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK){ // Verifies if the ESP-NOW communication protocol has not been initialized properly
    Serial.println("ESP-NOW Init Failed"); // Error message
    return; // Exits the setup function by returning nothing
  }

  // This function makes the microcontroller send data using the onDataSent function and the ESP-NOW communication protocol, acting as an interrupt to the current task
  esp_now_register_send_cb(onDataSent);

  // Adds any receivers as a peer to this microcontroller
  addPeer(receiver1); 
  addPeer(receiver2);

  // Sets the device name that the phone will see
  NimBLEDevice::init("SmokeAlarmListener");

  NimBLEServer *pServer = NimBLEDevice::createServer(); // Turns the ESP32 into a BLE Server
  pServer->setCallbacks(new MyServerCallbacks()); // Links the custom class to BLE events

  /*
    Creates a BLE service, which acts like a container/category for related data
    The characterirstic is attached to the service
    READ allows the phone to request a value, and notify allows the ESP32 to push updates
    The CCCD (Client Characteristic Configuration Descriptor) here is 2902
    A phone, a client device, can enable and disable notifications or indications for this characteristic. 
  */ 
  NimBLEService *pService = pServer->createService("1234");
  alarmCharacteristic = pService->createCharacteristic("5678", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  alarmCharacteristic->createDescriptor("2902"); 

  // Makes the service active
  pService->start();

  // Begins advertising, allowing other devices to now connect to it
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("1234"); // Helps the phone discover the service
  pAdvertising->start(); // Starts the advertising

  Serial.println("BLE System Ready"); // Debug message for confirmation of BLE
}

void loop(){
  /*
    This if statement checks if one sample period has elapsed, and then allows a new sample
    The micros() function obtains the current time in microseconds
    1,000,000 is in microseconds which is equal to 1 second
    It is divided by the sample rate to obtain the sample period
    Currently: 1,000,000 µs / 2000 samples per second = 500 µs, or 1 sample every 500 µs
  */
  if(micros() - lastSampleTime >= (1000000 / SAMPLE_RATE)){
    lastSampleTime = micros(); // Creates a time stamp of the current time for sampling

    int sample = analogRead(ADC_PIN); // From the ADC pin, sample the voltage
    int amplitude = abs(sample - 2048); // Eliminates the DC offset and takes the absolute value to obtain only positive values

    // Converts analog signal amplitude into binary based on the threshold, with the states being sound or silence, with sound being on and silence being off
    currentSoundState = (amplitude > THRESHOLD); // True = on/1, false = off/0

    // Obtains the current time in milliseconds, to be used as a reference for when there is a change in states, on to off or off to on
    unsigned long currentTime = millis(); 

    // Detects a state transition, by comparing if the current sound state is not the same as the previous, indicating it has gone from either on to off or off to on
    if(currentSoundState != lastSoundState){
      unsigned long durationOfLastState = currentTime - lastStateTransitionTime; // Given a change in state, determine the duration that the signal was in that state
      lastStateTransitionTime = currentTime; // Stores the current time that the state changed to be used in the next loop to calculate the duration

      // If the last sound state was on, meaning transitoned from on to off. For pulses
      if(lastSoundState == true){
        if(durationOfLastState > PULSE_MIN && durationOfLastState < PULSE_MAX){ // Check if the duration that the pulse was on is in the proper timing window
          pulseCount++;
          Serial.println("Valid Pulse"); // Printing for testing verification
        } 
        else{ // The pulse did not meet that duration and is invalid
          pulseCount = 0; // Reset the pulse count given an invalid pulse. Acts as a reset given the period of the incoming signal, and also ensures no false positives
        }
      }
      // If the last sound state was off, meaning transitoned from off to on, For pauses
      else{ 
        // Checks if 3 pulses have occurred given the specific timing windows, and if the duration of the last state fits the the pause timing window
        if(pulseCount == 3 && durationOfLastState > PAUSE_MIN && durationOfLastState < PAUSE_MAX){
          Serial.println("T3 DETECTED"); // Temporal 3 has been detected, and the smoke detector alarm is going off
          lastT3Time = currentTime; // Record the current time that the T3 was detected
          pulseCount = 0; // Reset pulse count after successful T3 detection
        }
      }
    }

    lastSoundState = currentSoundState; // Record current sound state as the previous sound state after all of the checks for the next loop
  }

  unsigned long currentTime = millis(); // Obtain the current updated time in ms

  // Alarm Management Block:
  // The alarm has been active for less than the hold time
  if(currentTime - lastT3Time < ALARM_HOLD_TIME){
    alarmActive = true; // Activate/maintain the alarm as on
  } 
  else{
    alarmActive = false; // Deactivate/maintain the alarm as off
  }

  // Transmision Block:
  // If the time that a message was last sent exceeded the set time interval
  if(currentTime - lastSendTime > SEND_INTERVAL){
    lastSendTime = currentTime; // Updates the time that a message was sent

    outgoingData.smokeDetectorAlarmTriggered = alarmActive; // Trigger the flag for communication

    // ESP-NOW Transmission
    esp_now_send(receiver1, (uint8_t*)&outgoingData, sizeof(outgoingData)); // Send to receiver 1
    esp_now_send(receiver2, (uint8_t*)&outgoingData, sizeof(outgoingData)); // Send to recevier 2

    // BLE Transmission
    if(deviceConnected){ // Data can only be sent if the phone is connected
      uint8_t value; // A 1-byte value to send over BLE indicating 0 for no alarm, or 1 for alarm
      if(alarmActive){ // If the alarm is going off
        value = 1; // Value is true
      }
      else{ // If the alarm is not going off
        value = 0; // Value is false
      }
      alarmCharacteristic->setValue(&value, 1); // &value is a pointer to data, and 1 is the size in bytes
      alarmCharacteristic->notify(); // Sends a notification to the connected phone with the update byte value
    }

    // Print current alarm state, for testing
    Serial.print("Alarm State Sent: "); 
    Serial.println(alarmActive);
  }
}
