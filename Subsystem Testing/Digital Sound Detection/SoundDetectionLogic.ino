/*
  TEST: Sound Detection Logic

  PURPOSE:
  Validate sound detection logic that uses hysteresis thresholds

  METHOD:
  - Read ADC
  - Apply hysteresis thresholds
  - Print state transitions

  EXPECTED RESULT:
  - Stable ON/OFF transitions
  - No rapid flickering near threshold
*/

#define ADC_PIN 1
#define THRESHOLD_HIGH 220
#define THRESHOLD_LOW 180

bool soundState = false;

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
}

void loop(){
  int sample = analogRead(ADC_PIN);
  int amplitude = abs(sample - 2048);

  if(soundState){
    if(amplitude < THRESHOLD_LOW){
      soundState = false;
      Serial.println("OFF");
    }
  } else {
    if(amplitude > THRESHOLD_HIGH){
      soundState = true;
      Serial.println("ON");
    }
  }

  delay(1);
}