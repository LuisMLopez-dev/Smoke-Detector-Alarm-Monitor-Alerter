#include <WiFi.h> // Control of wireless network connections for Wi-Fi enabled microcontrollers
#include <esp_now.h> // Wireless communication protocol

#define MOTOR_PIN 4 // GPIO 4, PWM output to the vibration motor driver

// PWM Settings
#define PWM_FREQ 5000 // PWM frequency in Hz. 5 kHz is well above audible range for the motor and avoids whine
#define PWM_RES 8 // PWM resolution in bits. 8 bits gives duty values from 0 to 255

#define START_DUTY 230 // Higher duty cycle applied briefly on motor startup to overcome static friction (startup kick)
#define RUN_DUTY 200 // Lower duty cycle used to sustain the motor once it is spinning, reducing current draw
#define START_TIME 200 // Duration in ms to hold the startup kick before dropping to RUN_DUTY

unsigned long lastRecv = 0; // Stores the time of the last received data, in milliseconds
const unsigned long timeout = 5000; // Timing for a failsafe to trigger alarms if communication protocols were to fail and exceed this time, in ms (1000 ms = 1 s)

// Motor State Tracking
bool alarmState = false; // Current alarm state from the received message. True = alarm is active
bool motorRunning = false; // Tracks whether the motor is currently running, used to detect the OFF to ON transition for the startup kick
unsigned long motorStartTime = 0; // Stores the time the motor was started, used to time the startup kick

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

  // Motor Control Logic
  if (alarmState == true){ // If the alarm is active, the motor should be running

    // If the motor is off, and it just turned ON based on the alarm state
    if (motorRunning == false){
      motorRunning = true; // Track that the motor is now running
      motorStartTime = millis(); // Record the time the motor started, used for the startup kick timing

      // Startup kick: apply a higher duty cycle briefly to overcome static friction and get the motor spinning reliably
      ledcWrite(MOTOR_PIN, START_DUTY);
    }

    // After the startup kick window has elapsed, switch to the lower run duty for lower current draw
    if (millis() - motorStartTime > START_TIME){
      ledcWrite(MOTOR_PIN, RUN_DUTY);
    }
  }
  else{ // If the alarm is not active
    motorRunning = false; // Track that the motor is not running
    ledcWrite(MOTOR_PIN, 0); // Turn and maintain motor OFF
  }

  Serial.println(alarmState); // Prints the current alarm state for testing verification

  delay(10); // Very small delay to prevent the loop from running too fast and flooding the serial output
}
