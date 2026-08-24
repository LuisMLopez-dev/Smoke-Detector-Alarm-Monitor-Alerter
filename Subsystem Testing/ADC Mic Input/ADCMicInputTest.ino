/*
  TEST: ADCMicInputTest

  PURPOSE:
  Verify that the MAX9814 microphone module outputs a valid signal and that the ESP32 ADC can read and display it.

  METHOD:
  - Continuously sample ADC
  - Print raw and centered amplitude to Serial Plotter

  EXPECTED RESULT:
  - Signal is near midpoint (~2048)
  - Audio produces visible waveform variations
*/

#define ADC_PIN 1

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
}

void loop(){
  int sample = analogRead(ADC_PIN);
  int amplitude = abs(sample - 2048);

  Serial.print(sample);
  Serial.print(",");
  Serial.println(amplitude);

  delay(1); // ~1 kHz sampling for plotting
}
