// ESP32 window controller: drives a continuous-rotation servo (MG995 360°) to
// open and close a window.
//
// A 360° servo has no position control: the pulse width sets speed and
// direction (1500 us = stop, lower = one way, higher = the other way). So a
// "90° move" is done by spinning for a fixed time, which needs calibrating.
//
// Test mode: the servo turns roughly 90° one way, pauses, turns back, and
// repeats forever. Progress is printed on the serial monitor (115200 baud).

#include <Arduino.h>
#include <ESP32Servo.h>

// ---------- Configuration ----------

constexpr int SERVO_PIN = 13;

// Pulse width at which the servo stands still. If it creeps while it should
// be stopped, adjust this by 5-10 us until it stays put.
constexpr int STOP_US = 1500;

// How far from STOP_US to drive while moving. Bigger = faster (max ~500).
constexpr int SPEED_US = 500;

// How long to spin for one ~90° move. Calibrate: if it turns too far, lower
// it; too little, raise it. Opening and closing may need different values.
constexpr unsigned long OPEN_MS = 300;
constexpr unsigned long CLOSE_MS = 1000;

// How long to wait at each end before turning around.
constexpr unsigned long PAUSE_AT_END_MS = 3000;

// ---------- State ----------

enum class Phase { Opening, PauseOpen, Closing, PauseClosed };

Servo servo;

Phase phase = Phase::PauseClosed;
unsigned long phaseStartMs = 0;

// ---------- Servo ----------

void startPhase(Phase next) {
  phase = next;
  phaseStartMs = millis();

  switch (phase) {
    case Phase::Opening:
      servo.writeMicroseconds(STOP_US + SPEED_US);
      Serial.println("Opening");
      break;
    case Phase::Closing:
      servo.writeMicroseconds(STOP_US - SPEED_US);
      Serial.println("Closing");
      break;
    case Phase::PauseOpen:
    case Phase::PauseClosed:
      servo.writeMicroseconds(STOP_US);
      Serial.println("Stopped");
      break;
  }
}

void updateServo() {
  unsigned long elapsed = millis() - phaseStartMs;

  switch (phase) {
    case Phase::Opening:
      if (elapsed >= OPEN_MS) startPhase(Phase::PauseOpen);
      break;
    case Phase::PauseOpen:
      if (elapsed >= PAUSE_AT_END_MS) startPhase(Phase::Closing);
      break;
    case Phase::Closing:
      if (elapsed >= CLOSE_MS) startPhase(Phase::PauseClosed);
      break;
    case Phase::PauseClosed:
      if (elapsed >= PAUSE_AT_END_MS) startPhase(Phase::Opening);
      break;
  }
}

// ---------- Main ----------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nWindow controller starting (sweep test)");

  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2500);
  startPhase(Phase::PauseClosed);
}

void loop() { updateServo(); }
