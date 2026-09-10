/*
  TEST: Temporal 3 Pattern Detection

  PURPOSE:
  Verify Temporal-3 detection using envelope-based sound detection system.

  METHOD:
  - Envelope follower for stable sound detection
  - Hysteresis on envelope
  - FSM timing validation for T3 pattern
  - Alarm latch to reduce false negatives
  - LED indicates alarm state

  EXPECTED RESULT:
  - Stable detection of smoke alarm T3 pattern
  - Minimal false positives
  - Reduced false negatives via latch behavior
*/

#define ADC_PIN 1
#define LED_PIN 4

#define ADC_MIDPOINT 1400

// Envelope and Hysteresis
#define THRESHOLD_HIGH 400
#define THRESHOLD_LOW 250
#define DECAY 0.95

#define MIN_VALID_STATE_TIME 60 // In ms

// Sampling
#define SAMPLE_RATE 8000
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE)

// T3 timing in ms
#define PULSE_MIN 200
#define PULSE_MAX 800

#define SHORT_PAUSE_MIN 200
#define SHORT_PAUSE_MAX 800

#define LONG_PAUSE_MIN 800
#define LONG_PAUSE_MAX 2000

#define ALARM_HOLD_TIME 8000

bool currentState = false;
bool lastState = false;

float envelope = 0;

unsigned long lastSampleTime = 0;
unsigned long lastTransition = 0;
unsigned long alarmLatchedTime = 0;
unsigned long stateStartTime = 0; // Track how long we’ve been in a valid state

int pulseCount = 0;
bool alarmActive = false;

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(LED_PIN, OUTPUT);
}

void loop(){
  unsigned long nowMicros = micros();

  // Sampling at fixed rate
  if(nowMicros - lastSampleTime >= SAMPLE_PERIOD_US){
    lastSampleTime = nowMicros;

    int sample = analogRead(ADC_PIN);
    int amplitude = abs(sample - ADC_MIDPOINT);

    // Envelope follower
    envelope = envelope * DECAY + amplitude * (1 - DECAY);

    // Hysteresis on envelope
    if(currentState){
      if(envelope < THRESHOLD_LOW){
        currentState = false;
      }
    }
    else{
      if(envelope > THRESHOLD_HIGH){
        currentState = true;
      }
    }

    unsigned long now = millis();

    // State transition detection
    if(currentState != lastState){
      unsigned long duration = now - lastTransition;

      if(duration >= MIN_VALID_STATE_TIME){ // Small delay to avoid any spikes
        lastTransition = now;

        // Update state timer only on valid transitions
        stateStartTime = now;

        if(lastState){
          // The beep ended
          if(duration > PULSE_MIN && duration < PULSE_MAX){
            pulseCount++;
            Serial.println("Pulse OK");
          }
          else{
            pulseCount = 0;
          }
        }
        else{ // The pause ended

          // Validate short pauses between beeps
          if(pulseCount > 0 && pulseCount < 3){
            if(duration < SHORT_PAUSE_MIN || duration > SHORT_PAUSE_MAX){
              pulseCount = 0;
            }
          }

          // Validate full T3 pattern
          if(pulseCount == 3){
            if(duration > LONG_PAUSE_MIN && duration < LONG_PAUSE_MAX){
              Serial.println("T3 DETECTED");
              alarmActive = true;
              alarmLatchedTime = now;
            }
            pulseCount = 0;
          }
        }

        lastState = currentState;
      }
    }

    // State-based timeout protection
    if(currentState){ // HIGH state handling
      if(now - stateStartTime > PULSE_MAX + 200){ // A HIGH (beep) should not exceed max pulse duration
        pulseCount = 0;
      }
    }
    else{ // LOW state handling
      if(pulseCount > 0 && pulseCount < 3){
        if(now - stateStartTime > SHORT_PAUSE_MAX + 150){ // Short pauses between pulses
          pulseCount = 0;
        }
      }
      else if(pulseCount == 3){
        if(now - stateStartTime > LONG_PAUSE_MAX + 200){ // Long pause at the end of a T3 cycle
          pulseCount = 0;
        }
      }
    }

    // Alarm Hold Logic
    if(alarmActive){
      if(now - alarmLatchedTime <= ALARM_HOLD_TIME){
        digitalWrite(LED_PIN, HIGH);
      }
      else{
        alarmActive = false;
        digitalWrite(LED_PIN, LOW);
      }
    }
    else{
      digitalWrite(LED_PIN, LOW);
    }

    // Debug Output for serial monitor and plotter
    Serial.print(envelope);
    Serial.print(",");
    Serial.print(currentState);
    Serial.print(",");
    Serial.print(pulseCount);
    Serial.print(",");
    Serial.println(alarmActive);
  }
}
