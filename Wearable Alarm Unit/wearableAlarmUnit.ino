#include <WiFi.h> // Control of wireless network connections for Wi-Fi enabled microcontrollers
#include <esp_now.h> // Wireless communication protocol

#define MOTOR_PIN 4 // GPIO 4, PWM output to the vibration motor driver

// PWM Settings
#define PWM_FREQ 5000 // PWM frequency in Hz. 5 kHz is well above audible range for the motor and avoids whine
#define PWM_RES 8 // PWM resolution in bits. 8 bits gives duty values from 0 to 255

#define RUN_DUTY 255 // Full duty

// Pulsed vibration
#define BURST_ON_MS 300 // Duration of each vibration burst, in ms
#define BURST_OFF_MS 200 // Duration of the pause between bursts, in ms
#define BURST_PERIOD_MS (BURST_ON_MS + BURST_OFF_MS) // Total burst cycle length

unsigned long lastRecv = 0; // Stores the time of the last received data, in milliseconds
const unsigned long timeout = 5000; // Timing for a failsafe to trigger alarms if communication protocols were to fail and exceed this time, in ms (1000 ms = 1 s)

// Motor State Tracking
bool alarmState = false; // Current alarm state from the received message. True = alarm is active

// Message Structure
typedef struct{ // Typedef is used here to make an alias for this struct
  bool alarm; // Flag for the smoke detector alarm, for communication
} Message; // Alias name for this struct

Message msg; // Variable to hold the received data

// Function to receive the MAC address of the transmitter device, the incoming data in bytes, and the length of the incoming data
void onRecv(const esp_now_recv_info*, const uint8_t* data, int len){

  if (len != sizeof(msg)){ // Ensures the data is of proper length
    return; // The data was not of proper length, so return nothing and exit this function
  }

  // Makes a copy of the data, data, to the destination, msg, and copies the number of bytes equal to the size of the struct
  memcpy(&msg, data, sizeof(msg));

  // Changes the alarm state based on the incoming data that alters the boolean flag
  alarmState = msg.alarm;

  lastRecv = millis(); // Obtains the current time of the currently received data, resetting the fail-safe timer
}

void setup(){
  Serial.begin(115200); // Baud rate: speed for serial communication

  // Sets the microcontroller in station mode, allowing it to send or receive data packets
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK){ // Verifies if the ESP-NOW communication protocol has not been initialized properly
    Serial.println("Error initializing ESP-NOW"); // Error message
    return; // Exits the setup function by returning nothing
  }

  // This function makes the microcontroller listen for incoming ESP-NOW data, and to interrupt the current task to run the onRecv function
  esp_now_register_recv_cb(onRecv);

  // Setup PWM for the motor using the LEDC peripheral
  ledcAttach(MOTOR_PIN, PWM_FREQ, PWM_RES); // Attaches the motor pin to an LEDC channel with the configured frequency and resolution
  ledcWrite(MOTOR_PIN, 0); // Ensures the motor starts in the OFF state
}

void loop(){
  // FAIL-SAFE CODE:
  // If more time than the timeout has passed since the last received message, force the alarm on
  if (millis() - lastRecv > timeout){
    alarmState = true; // Trigger Alarm
  }

  // Motor Control Logic: Pulsed vibration while the alarm is active, and pulsed to increase the noticeability of the vibrations
  if (alarmState == true){
    // The phase is equal to the remainder (0 to 499) of the current time in ms divided by the period of the burst
    unsigned long phase = millis() % BURST_PERIOD_MS;

    if (phase < BURST_ON_MS){ // If the phase is between 0 to 299, which is 300 total
      ledcWrite(MOTOR_PIN, RUN_DUTY); // Inside the burst window, turn the Motor ON
    }
    else{
      ledcWrite(MOTOR_PIN, 0); // Outside the burst window turn the motor OFF
    }
  }
  else{ // If the alarm is not active
    ledcWrite(MOTOR_PIN, 0); // Turn and maintain motor OFF
  }

  Serial.println(alarmState); // Prints the current alarm state for testing verification

  delay(10); // Very small delay to prevent the loop from running too fast and flooding the serial output
}
