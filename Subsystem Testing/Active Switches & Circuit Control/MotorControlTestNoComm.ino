/*
  TEST: Controlling a vibration motor using PWM and a MOSFET as an active switch without any wireless communication

  PURPOSE:
  Verify that the ESP32-S3 can reliably control a DC vibration motor using PWM while powered from the +3.3 V rail. This test is to ensure that the motor can:
  - Start reliably
  - Run at a controlled (reduced) power level
  - Turn OFF cleanly

  METHOD:
  - Use PWM (LEDC) to control motor power through a MOSFET
  - Apply a short high-power "startup kick" to ensure motor spins
  - Reduce to a medium duty cycle for sustained operation
  - Turn OFF after a fixed duration

  EXPECTED RESULTS:
  - Motor starts immediately at high power
  - Motor continues running at reduced intensity
  - No ESP32 resets or instability
  - Motor turns OFF cleanly after delay

  NOTES:
  - This test validates that the motor can be powered directly from the 3.3 V rail
  - PWM is used for control, not strictly required for operation
*/

#define CONTROL_PIN 4

// PWM settings
#define PWM_FREQ 5000 // PWM frequency in hertz (Hz)
#define PWM_RES 8 // PWM resolution (8-bit → values from 0 to 255)

// Tunable parameters
#define START_DUTY 230 // High duty cycle (~90%) for reliable startup
#define RUN_DUTY 160 // Medium duty cycle (~63%) for sustained operation
#define START_TIME 200 // Duration of startup kick in milliseconds

void setup(){
  // Attaches a PWM signal to the control pin
  ledcAttach(CONTROL_PIN, PWM_FREQ, PWM_RES);

  // Ensures the motor is OFF at startup, so 0% duty cycle
  ledcWrite(CONTROL_PIN, 0);
}

void loop(){
  // Apply a high duty cycle to overcome inertia and resistor voltage drop to guarantee motor startup 
  ledcWrite(CONTROL_PIN, START_DUTY);
  delay(START_TIME);

  // Reduces the duty cycle to a lower current while maintaining vibration
  ledcWrite(CONTROL_PIN, RUN_DUTY);
  delay(3000);

  // Set duty cycle to 0 so that the motor turns OFF
  ledcWrite(CONTROL_PIN, 0);
  delay(3000);
}
