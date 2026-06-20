#include <WiFi.h> // Control of wirelesss network connections for Wi-Fi enabled microcontrollers
#include <esp_now.h> // Wireless communcation protocol

// GPIO 4
#define outputPin 4 

typedef struct{ //Typedef is used here to make an alias for this struct
  bool smokeDetectorAlarmTriggered; // Flag for the smoke detector alarm, for communication
} structMessage; // Alias name for this struct

structMessage incomingData = {false}; // Variable to hold the received data, default state set known state of false

bool alarmState = false; // Initial known state of alarm is off
unsigned long lastReceivedTime = 0; // Stores the time of the last received data
const unsigned long timeout = 5000; // Timing for a failsafe to trigger alarms if communication protocols were to fail and exceed this time, in ms (1000 ms = 1 s)

// Function to receive the mac address of the transmitter device, the incoming data in bytes, and the length of the incoming data
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataBytes, int length){
  if(length == sizeof(incomingData)){ // Ensures the data is of proper length
  // Makes a copy of the data, incomingDataBytes, to the destination, incomingData, and copies the number of bytes equal to the size of the struct
    memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));
    lastReceivedTime = millis(); // Obtains the current time of the currently received data
  }
  else{
    return; // The data was not of proper length, so return nothing and exit this function
  }
  
  // Changes the alarm state based on the incoming data that alters the boolean flag
  alarmState = incomingData.smokeDetectorAlarmTriggered;

  // Future Testing Notes: Can be modified for less serial spam, or removed once verified
  // Prints the alarm state to verify in testing that the communication was successful and matches the state of the alarm
  Serial.print("Alarm State: "); 
  Serial.println(incomingData.smokeDetectorAlarmTriggered);
}

void setup(){
  Serial.begin(115200); // Baud rate: speed for serial communication

  pinMode(outputPin, OUTPUT); // Establsihes the outputPin as an output
  digitalWrite(outputPin, LOW); // Initialized defined state of low 

  // Initialize timing
  lastReceivedTime = millis(); 

  // Sets the microcontroller in station mode, allowing to to send or recieve data packets
  WiFi.mode(WIFI_STA); 

  if(esp_now_init() != ESP_OK){ // Verifies if the ESP-NOW communication protocol has not been initialized properly
    Serial.println("Error initializing ESP-NOW communication"); // Error message
    return; // Exits the setup function by returning nothing
  }

  // This function makes the microcontroller listen for incoming ESP-NOW data, and to interrupt the current taks to run the onDataRecv function
  esp_now_register_recv_cb(onDataRecv); 
}

void loop(){
  /* 
    Possible Future Changes & Testing Notes: 
    Fail-safe code below should be commented out to test the communication protocols, and activation of the output pin. It should be tested after this.
    After testing, the code below may have to be adjusted to be dependent on the state of the received message and time, instead of just time itself
    This would require restructing of how the alarm of this device is triggered. Proper testing is required first before making such changes
  */ 

  // FAIL-SAFE CODE:
  if(millis() - lastReceivedTime > timeout){ //If more time that then timeout time has passed
    alarmState = true; // Trigger Alarm
  }

  // The output is controlled based on the alarm state
  digitalWrite(outputPin, alarmState);
}
