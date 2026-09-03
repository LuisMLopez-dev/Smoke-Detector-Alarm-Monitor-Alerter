/*
  TEST: ADC Mic Input Test

  PURPOSE:
  Verify that the MAX9814 microphone module outputs a valid signal and that the ESP32 ADC can read and display it.

  METHOD:
  - Continuously sample ADC
  - Print raw and centered amplitude to Serial Plotter

  EXPECTED RESULT:
  - Signal is near midpoint value of about 1400, which was configured based on initial readings from the mcu
  - Audio produces visible waveform variations
*/

#define ADC_PIN 1
#define ADC_MIDPOINT 1400  // Based on initial readings, the midpoint is at this value

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
}

#define SAMPLE_RATE 8000  // 8 kHz target
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)

void loop(){
  static unsigned long lastSampleTime = 0;
  unsigned long now = micros();

  if (now - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = now;

    int sample = analogRead(ADC_PIN);
    int amplitude = abs(sample - ADC_MIDPOINT);

    Serial.print(sample);
    Serial.print(",");
    Serial.println(amplitude);
  }
}
