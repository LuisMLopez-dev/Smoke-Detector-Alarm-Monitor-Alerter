/*
  TEST: Sound Detection Logic using an envelope with hysteresis thresholds

  PURPOSE:
  Validate stable sound detection using an envelope follower combined with hysteresis thresholds

  METHOD:
  - Sample the ADC readings from the MAX9814 microphone
  - Convert to an amplitude using a fixed midpoint
  - Apply envelope smoothing, which is a digital low-pass filter
  - Apply hysteresis thresholds to determine soundState
  - Output raw, envelope, and sound state to serial plotter
  - Drive an LED based on detected sound state

  EXPECTED RESULT:
  - Smooth envelope signal during sound events
  - Stable ON/OFF transitions, with no flickering
  - LED remains ON for duration of sustained sounds, such as alarm beeps
  - Minimal triggering from normal speech or background noise
*/

#define ADC_PIN 1
#define LED_PIN 4
#define ADC_MIDPOINT 1400 

#define THRESHOLD_HIGH 400
#define THRESHOLD_LOW 250

#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)

#define DECAY 0.97 // Can be adjusted between 0.90 and 0.99. This test sketch worked at this value.

bool soundState = false;
int statePlot; // For visual purposes only, so that when soundState is true, it is higher than everything else on the serial plotter
float envelope = 0; // Envelope based detection

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(LED_PIN, OUTPUT);
}

void loop(){
  static unsigned long lastSampleTime = 0;
  unsigned long now = micros();

  if (now - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = now;

    int sample = analogRead(ADC_PIN);
    int amplitude = abs(sample - ADC_MIDPOINT);

    // Envelope Follower
    envelope = envelope * DECAY + amplitude * (1 - DECAY);

    // Hysteresis on envelope
    if(soundState){
      if(envelope < THRESHOLD_LOW){
        soundState = false;
        digitalWrite(LED_PIN, LOW);
      }
    } 

    else{
      if(envelope > THRESHOLD_HIGH){
        soundState = true;
        digitalWrite(LED_PIN, HIGH);
      }
    }

    if (soundState){
      statePlot = 800;
    } 
    else{
      statePlot = 0;
    }

    // For serial plotter
    Serial.print(amplitude); // Raw values
    Serial.print(",");
    Serial.print(envelope); // Smoothed
    Serial.print(",");
    Serial.println(statePlot);
  }
}
