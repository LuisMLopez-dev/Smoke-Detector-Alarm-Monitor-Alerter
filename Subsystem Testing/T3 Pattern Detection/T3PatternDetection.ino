/*
  TEST: T3_PATTERN_DETECTION

  PURPOSE:
  Verify Temporal-3 detection using microphone input.

  METHOD:
  - Detect ON/OFF states using hysteresis thresholds 
  - Measure pulse durations
  - Count valid pulses

  EXPECTED RESULT:
  - "T3 DETECTED" printed for valid pattern
*/

#define ADC_PIN 1
#define THRESHOLD_HIGH 220
#define THRESHOLD_LOW 180

#define PULSE_MIN 300
#define PULSE_MAX 700
#define PAUSE_MIN 1200
#define PAUSE_MAX 2000

bool currentState = false;
bool lastState = false;

unsigned long lastTransition = 0;
int pulseCount = 0;

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
}

void loop(){
  int sample = analogRead(ADC_PIN);
  int amplitude = abs(sample - 2048);

  // Hysteresis Thresholds
  if(currentState){
    if(amplitude < THRESHOLD_LOW) 
      currentState = false;
  } 
  else{
    if(amplitude > THRESHOLD_HIGH) 
      currentState = true;
  }

  unsigned long now = millis();

  if(currentState != lastState){
    unsigned long duration = now - lastTransition;
    lastTransition = now;

    if(lastState){ // Beep/pulse ended
      if(duration > PULSE_MIN && duration < PULSE_MAX){
        pulseCount++;
        Serial.println("Pulse OK");
      } 
      else{
        pulseCount = 0;
      }
    } 
    else{ // Silence/pause ended
      if(pulseCount == 3 && duration > PAUSE_MIN && duration < PAUSE_MAX){
        Serial.println("T3 DETECTED");
        pulseCount = 0;
      }
    }
  }

  lastState = currentState;
}