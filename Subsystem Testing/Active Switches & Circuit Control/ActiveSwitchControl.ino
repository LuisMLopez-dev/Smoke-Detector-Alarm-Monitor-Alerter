/*
  TEST: Controlling a load through an Active Switch (Transistor)

  PURPOSE:
  Verify that the microcontroller can control an active switch, BJT and a MOSFET to drive a load such as an LED or vibration motor.

  METHOD:
  - Toggle the GPIO output at a fixed interval
  - Observe the load behavior of the LEDs in the LED array and of the DC vibration puck motor

  EXPECTED RESULT:
  - Load turns ON when GPIO is HIGH
  - Load turns OFF when GPIO is LOW
  - No erratic switching or instability

  NOTES:
  - For the BJT: GPIO drives base through resistor
  - For the MOSFET: GPIO drives gate through resistor
  - Ensure that a proper flyback diode is used for inductive loads, such as the vibration motor
*/

#define CONTROL_PIN 4

void setup(){
  pinMode(CONTROL_PIN, OUTPUT);
  digitalWrite(CONTROL_PIN, LOW); // Ensure known startup state 
}

void loop(){
  digitalWrite(CONTROL_PIN, HIGH); // Turn load ON
  delay(2000);

  digitalWrite(CONTROL_PIN, LOW);  // Turn load OFF
  delay(2000);
}