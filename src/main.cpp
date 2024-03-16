#include <Arduino.h>

// Library for pin-change interrupts
// https://github.com/GreyGnome/EnableInterrupt
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
const unsigned int PLAYER_1_FREQ = 494; // B4
const unsigned int PLAYER_2_FREQ = 523; // C5
const unsigned int PLAY_FREQ = 440; // A4
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
volatile enum Player {PLAYER_1, PLAYER_2, NONE} winningPlayer;
volatile unsigned long buttonPressAt[2] = {0, 0};
volatile unsigned long startReactionAt;
volatile unsigned long lastStateTransitionAt;
volatile bool playerError;

void print(const char* msg);
void playerButtonPressed(Player player);
void player1ButtonPressed() { playerButtonPressed(PLAYER_1); }
void player2ButtonPressed() { playerButtonPressed(PLAYER_2); }
bool nbDelay(unsigned int ms);
void transitionState(State newState, bool debounce = false);

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
        for(int isOn = 1; isOn >= 0; isOn--) {
          for(int j = 0; j < 2; j++) {
            digitalWrite(j ? PLAYER_2_LED : PLAYER_1_LED, isOn);
            tone(BUZZER_PIN, j ? PLAYER_2_FREQ : PLAYER_1_FREQ); 
            nbDelay(200);
          }
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
        tone(BUZZER_PIN, winningPlayer == PLAYER_1 ? PLAYER_1_FREQ : PLAYER_2_FREQ);
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

void playerButtonPressed(Player player) {
  buttonPressAt[player] = millis();
  switch(state) {
    case PLAYING: {
      playerError = true;
      winningPlayer = player == PLAYER_1 ? PLAYER_2 : PLAYER_1;
      transitionState(SCORING);
      break;
    }
    case REACTING: {      
      winningPlayer = player;
      transitionState(SCORING);
      break;
    }
    case READY:
    case POST_SCORING:
    case SCORING: {
      unsigned long delta = buttonPressAt[0] - buttonPressAt[1];
      if (abs(delta) < DEBOUNCE_MS) {
        transitionState(PRE_PLAY, true);
      }
      break;
    }

    default: {
      break;
    }
  }
}


// delays for ms milliseconds, but returns early if the state has changed
// returns true if interrupted with a state change, false if the delay completes without interruption
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