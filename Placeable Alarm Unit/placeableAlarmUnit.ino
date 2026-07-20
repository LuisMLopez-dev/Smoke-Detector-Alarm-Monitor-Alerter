#include <WiFi.h> // Control of wirelesss network connections for Wi-Fi enabled microcontrollers
#include <esp_now.h> // Wireless communcation protocol

// GPIO 4
#define outputPin 4 

typedef struct{ // Typedef is used here to make an alias for this struct
  bool smokeDetectorAlarmTriggered; // Flag for the smoke detector alarm, for communication
} structMessage; // Alias name for this struct

structMessage incomingData = {false}; // Variable to hold the received data, default state set known state of false

volatile bool alarmState = false; // Initial known state of alarm is off. Set to volatile because it is a global variable that is shared between functions
volatile unsigned long lastReceivedTime = 0; // Stores the time of the last received data. Set to volatile because it is a global variable that is shared between functions
const unsigned long timeout = 5000; // Timing for a failsafe to trigger alarms if communication protocols were to fail and exceed this time, in ms (1000 ms = 1 s)

// Output Handling of LEDs
unsigned long lastBlinkTime = 0; // Last time that the LEDs blinked
const unsigned long blinkInterval = 200; // The interval of blinking in ms
bool LEDState = false; // True = LED on, False = LED off
bool previousAlarmState = false;

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

  // Turns the LEDs on the moment the alarmState becomes true. This is for the edge case so that we don't wait the blinkInterval before first turning on the LEDs
  if(alarmState && !previousAlarmState){ // If the alarm is on and it was previously off, meaning the alarm just turned on. This condition will only be satsified when the alarm just turned on
    lastBlinkTime = millis(); // Store the current time in ms for comparisons in 2nd if statement below
    LEDState = true; // The LEDs are turned on, so the state is true
    digitalWrite(outputPin, HIGH); // Turn the LEDs on
  }

  if(alarmState){ // If the alarm is on
    unsigned long currentTime = millis(); // Takes the current time in ms, to be used as a compairson in the if statement below

    if(currentTime - lastBlinkTime >= blinkInterval){ // If more than 200 ms have passed since the LED blinked, then control the LED output
      lastBlinkTime = currentTime; // Stores the current time as the last time the LED blinked, so that this can be used for the iteration of the loop() for the if statement
      LEDState = !LEDState; // Switches the LED state to the opposite state so that the LED can be on or off for 200 ms
      digitalWrite(outputPin, LEDState); // The LEDs are controlled by the LEDState so that they are either on or off for 200 ms, making them flash each time this if statement is ran
    }

  }
  else{ // The alarm is not on
    digitalWrite(outputPin, LOW); // Turns the LEDs off
    LEDState = false; // Sets the state to false because the LEDs are not on, and also resets it in the case that the alarm was on, so that LEDState is handled properly in the nested if 
  }

  // Changes the previousAlarmState to be the current alarmState each iteration of the loop(), to handle whether the alarm just turned on after being off
  previousAlarmState = alarmState; 
}
