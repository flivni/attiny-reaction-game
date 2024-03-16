#include <Arduino.h>
// https://github.com/GreyGnome/EnableInterrupt?utm_source=platformio&utm_medium=piohome
#include "EnableInterrupt.h"

#if defined(__AVR_ATmega328P__)
const uint8_t BUZZER_PIN = 10; 
const uint8_t PLAYER_1_LED = 5;
const uint8_t PLAYER_2_LED = 6; 
const uint8_t PLAYER_1_BUTTON = 4; 
const uint8_t PLAYER_2_BUTTON = 3; 
#elif defined(__AVR_ATtiny85__)
const uint8_t BUZZER_PIN = 0;
const uint8_t PLAYER_1_LED = 1;
const uint8_t PLAYER_2_LED = 2;
const uint8_t PLAYER_1_BUTTON = 4;
const uint8_t PLAYER_2_BUTTON = 3;
#else
#error "Unsupported platform!"
#endif

const uint8_t DEBOUNCE_MS = 100;
const unsigned int PLAY_FREQ = 440; // A4
const unsigned int PLAYER_1_FREQ = 494; // B4;
const unsigned int PLAYER_2_FREQ = 523; // C4
const unsigned int ERROR_FREQ = 900; 

// STARTUP
//   - flash led's in order twice, transition to READY
// READY
//   - Wait for both buttons to be pressed within 50ms of each other, transition to PRE_PLAY
// PRE_PLAY
//   - Flash all lights together once, set startReactionAt to be within 5-7 seconds, transition to PLAYING
// PLAYING
//   - Wait until startReactionAt, transition to REACTING
// REACTING
//   - Flash red light, wait for either button to be pressed. When either button is pressed, record winner
//     and transition to SCORING.
volatile enum State {STARTUP, READY, PRE_PLAY, PLAYING, REACTING, SCORING, POST_SCORING} state;
volatile enum Player {NONE = -1, PLAYER_1 = 0, PLAYER_2 = 1} winningPlayer;
volatile unsigned long button1PressAt;
volatile unsigned long button2PressAt;
volatile unsigned long startReactionAt;
volatile unsigned long lastStateTransitionAt;
volatile bool playerError;

void player1ButtonPressed();
void player2ButtonPressed();
bool nbDelay(unsigned int ms);
void transitionState(State newState, bool debounce = false);
void print(const char* msg);

void setup() {
  #ifdef __AVR_ATmega328P__
    Serial.begin(9600);
  #endif

  transitionState(STARTUP);
  winningPlayer = NONE;

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PLAYER_1_LED, OUTPUT);
  pinMode(PLAYER_2_LED, OUTPUT);

  pinMode(PLAYER_1_BUTTON, INPUT_PULLUP);
  pinMode(PLAYER_2_BUTTON, INPUT_PULLUP);

  enableInterrupt(PLAYER_1_BUTTON, player1ButtonPressed, CHANGE);
  enableInterrupt(PLAYER_2_BUTTON, player2ButtonPressed, CHANGE);
}

void loop() {
  switch(state)
  {
    case STARTUP: {
      for(int i = 0; i < 2; i++) {
        for(int val = 1; val >= 0; val--) {
          digitalWrite(PLAYER_1_LED, val);
          tone(BUZZER_PIN, PLAYER_1_FREQ); 
          nbDelay(200);
          digitalWrite(PLAYER_2_LED, val);
          tone(BUZZER_PIN, PLAYER_2_FREQ);

          nbDelay(200);
        }
      }
      noTone(BUZZER_PIN);
      transitionState(READY);
      break;
    }
    case READY: {
      break;
    }
    case PRE_PLAY: {
      playerError = false;
      for(int val = 1; val >= 0; val--) {
        digitalWrite(PLAYER_1_LED, val);
        digitalWrite(PLAYER_2_LED, val);
        tone(BUZZER_PIN, PLAY_FREQ);
        nbDelay(200);
      }
      noTone(BUZZER_PIN);
      startReactionAt = millis() + random(5000, 7000);
      transitionState(PLAYING);
      break;
    }
    case PLAYING: {
      if(millis() >= startReactionAt) {
        digitalWrite(PLAYER_1_LED, HIGH);
        digitalWrite(PLAYER_2_LED, HIGH);
        transitionState(REACTING);
      }
      break;
    }
    case REACTING:
      // nothing doing
      break;

    case SCORING: {
      if(winningPlayer == PLAYER_1) {
        digitalWrite(PLAYER_1_LED, HIGH);
        digitalWrite(PLAYER_2_LED, LOW);
      } else {
        digitalWrite(PLAYER_1_LED, LOW);
        digitalWrite(PLAYER_2_LED, HIGH);
      }
      if (playerError) {
          for(int i = 0; i < 2; i++) {
            tone(BUZZER_PIN, ERROR_FREQ);
            nbDelay(200);
            noTone(BUZZER_PIN);
            nbDelay(100);
          }
      } else {
        if(winningPlayer == PLAYER_1) {
            tone(BUZZER_PIN, PLAYER_1_FREQ);
        } else {
            tone(BUZZER_PIN, PLAYER_2_FREQ);
        }
        nbDelay(500);
        noTone(BUZZER_PIN);
      }
      transitionState(POST_SCORING);
      break;
    }
    case POST_SCORING: {
      if (millis() - lastStateTransitionAt > 5000) {
        digitalWrite(PLAYER_1_LED, LOW);
        digitalWrite(PLAYER_2_LED, LOW);
        transitionState(READY);
      }
      break;
    }
  }
}

void player1ButtonPressed() {
  button1PressAt = millis();
  switch(state) {
    case REACTING: {
      winningPlayer = PLAYER_1;
      transitionState(SCORING);
      break;
    }
    case PLAYING: {
      playerError = true;
      winningPlayer = PLAYER_2;
      transitionState(SCORING);
      break;
    }
    case READY:
    case POST_SCORING:
    case SCORING: {
      unsigned long delta = button1PressAt - button2PressAt;
      if (abs(delta) < 100) {
        transitionState(PRE_PLAY, true);
      }
      break;
    }    
    default: {
      break;
    }
  }
}

void player2ButtonPressed() {
  button2PressAt = millis();

  switch(state) {
    case REACTING: {
      winningPlayer = PLAYER_2;
      transitionState(SCORING);
      break;
    }
    case PLAYING: {
      playerError = true;
      winningPlayer = PLAYER_1;
      transitionState(SCORING);
      break;
    }
    case READY:
    case POST_SCORING:
    case SCORING: {
      unsigned long delta = button2PressAt - button1PressAt;
      if (abs(delta) < 100) {
        transitionState(PRE_PLAY, true);
      }
      break;
    }

    default: {
      break;
    }
  }
}

// delays for ms milliseconds, but returns early if the state changes
// returns true if interrupted with a state change, false if the delay completes
bool nbDelay(unsigned int ms) {
 State currentState = state;
  unsigned long startMs = millis();
  while (currentState == state) {
    if ((millis() - startMs) >= ms) {
      return false;
    }
    delay(1);
  }
  return true;
}

void transitionState(State newState, bool debounce) {
  print("Transitioning state");
  if (debounce && ((millis() - lastStateTransitionAt) < DEBOUNCE_MS)) {
    return;
  }
  state = newState;
  lastStateTransitionAt = millis();
}

void print(const char* msg) {
  #ifdef __AVR_ATmega328P__
    Serial.println(msg);
  #endif
}