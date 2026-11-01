// ESP32 window controller: drives a continuous-rotation servo (MG995 360°) to
// open and close a window.
//
// A 360° servo has no position control: the pulse width sets speed and
// direction (1500 us = stop, lower = one way, higher = the other way). So a
// move is done by spinning for a fixed time, which needs calibrating.
//
// A DS18B20 thermometer is read regularly. When the temperature drops below
// CLOSE_BELOW_C, the servo turns the closing way for CLOSE_MS and stops.
// Progress is printed on the serial monitor (115200 baud).
//
// To save battery, the servo gets no signal while it isn't moving. In
// LOW_POWER mode the board also deep-sleeps between readings, and once the
// window is closed it sleeps until the re-arm button is pressed. Every wake-up
// restarts the program; the window only gets closed right before the final
// sleep, so after any wake-up it counts as open.

#include <Arduino.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <OneWire.h>
#include <driver/rtc_io.h>

// ---------- Configuration ----------

constexpr int SERVO_PIN = 26;

// DS18B20 data pin. Needs a 4.7 kOhm pull-up resistor to 3.3 V.
constexpr int TEMP_PIN = 5;

// Close the window when the temperature drops below this (°C).
constexpr float CLOSE_BELOW_C = 26.0;

// Battery mode: wake every SLEEP_INTERVAL_S to read the thermometer once and
// deep-sleep in between. Set to false while developing to stay awake and read
// every TEMP_INTERVAL_MS.
constexpr bool LOW_POWER = false;
constexpr uint64_t SLEEP_INTERVAL_S = 60;

// How often to read the thermometer when LOW_POWER is off.
constexpr unsigned long TEMP_INTERVAL_MS = 1000;

// Re-arm button, wired between this pin and GND. It wakes the board from deep
// sleep, so it must be an RTC pin (0, 2, 4, 12-15, 25-27, 32-39).
constexpr int BUTTON_PIN = 4;

// CPU clock. 80 MHz draws about half of the default 240 MHz and is the lowest
// that keeps serial and the servo timing reliable.
constexpr int CPU_MHZ = 80;

// Pulse width at which the servo stands still. If it creeps while it should
// be stopped, adjust this by 5-10 us until it stays put.
constexpr int STOP_US = 1500;

// How far from STOP_US to drive while moving. Bigger = faster (max ~500).
constexpr int SPEED_US = 100;

// How long to spin to close the window. Calibrate on the real window.
constexpr unsigned long CLOSE_MS = 1000;

// Test mode: run the closing move every TEST_INTERVAL_MS, ignoring the
// temperature, and never sleep. Set to false for normal operation.
constexpr bool TEST_LOOP = false;
constexpr unsigned long TEST_INTERVAL_MS = 6000;

// Test mode has to stay awake to keep looping.
constexpr bool SLEEPS = LOW_POWER && !TEST_LOOP;

// ---------- State ----------

Servo servo;
OneWire oneWire(TEMP_PIN);
DallasTemperature thermometer(&oneWire);

// No position feedback, so assume the window is open at boot.
bool windowClosed = false;
bool closing = false;
unsigned long closeStartMs = 0;
unsigned long lastTempRequestMs = 0;
unsigned long lastTestCloseMs = 0;

// ---------- Sleep ----------

// Deep-sleeps until the button is pressed, or for `seconds` if not 0. Waking
// from deep sleep starts the program again from setup().
void deepSleep(uint64_t seconds) {
  if (seconds) {
    Serial.printf("Sleeping for %llu s\n", seconds);
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  } else {
    Serial.println("Sleeping until the button is pressed");
  }
  Serial.flush();

  // Keep the servo signal low while asleep; a floating pin can make it twitch.
  gpio_hold_en((gpio_num_t)SERVO_PIN);
  gpio_deep_sleep_hold_en();

  rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  esp_deep_sleep_start();
}

// ---------- Servo ----------

// Stops the pulses entirely, so the servo draws less and can't hum or creep.
void stopServo() {
  servo.detach();
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
}

void startClosing() {
  closing = true;
  closeStartMs = millis();
  servo.attach(SERVO_PIN, 500, 2500);
  servo.writeMicroseconds(STOP_US + SPEED_US);
  Serial.println("Closing");
}

void updateServo() {
  if (!closing || millis() - closeStartMs < CLOSE_MS) return;
  closing = false;
  windowClosed = true;
  stopServo();
  Serial.println("Closed");
  // Nothing is left to do once the window is closed.
  if (SLEEPS) deepSleep(0);
}

void updateTestLoop() {
  if (!TEST_LOOP || closing) return;
  unsigned long now = millis();
  if (now - lastTestCloseMs < TEST_INTERVAL_MS) return;
  lastTestCloseMs = now;
  startClosing();
}

// ---------- Thermometer ----------

// Prints a reading and starts closing if it's cold enough.
void checkTemperature(float tempC) {
  // -127 means no sensor answered; 85 is the power-on value before the first
  // conversion finished.
  if (tempC == DEVICE_DISCONNECTED_C || tempC == 85.0) {
    Serial.println("Thermometer not ready or not connected");
    return;
  }

  Serial.printf("Temperature: %.2f C\n", tempC);

  if (tempC < CLOSE_BELOW_C && !windowClosed && !closing) {
    Serial.printf("Below %.1f C\n", CLOSE_BELOW_C);
    startClosing();
  }
}

// Reads the previous conversion and starts the next one, so loop() never
// waits the ~750 ms a 12-bit conversion takes. When sleeping, setup() takes
// the one reading per wake-up instead.
void updateTemperature() {
  if (SLEEPS) return;
  unsigned long now = millis();
  if (now - lastTempRequestMs < TEMP_INTERVAL_MS) return;
  lastTempRequestMs = now;

  float tempC = thermometer.getTempCByIndex(0);
  thermometer.requestTemperatures();
  checkTemperature(tempC);
}

// ---------- Main ----------

void setup() {
  setCpuFrequencyMhz(CPU_MHZ);
  Serial.begin(115200);
  delay(200);
  Serial.println("\nWindow controller starting");
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woken by button: window counts as open again");
  }

  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  stopServo();
  // Release the low hold from deep sleep, now that the pin is driven low.
  gpio_hold_dis((gpio_num_t)SERVO_PIN);

  thermometer.begin();
  Serial.printf("Found %d thermometer(s)\n", thermometer.getDeviceCount());

  if (SLEEPS) {
    // Woke up for one reading: wait for it (~750 ms), act on it, sleep again.
    // If it starts closing, loop() finishes the move and then sleeps.
    thermometer.requestTemperatures();
    checkTemperature(thermometer.getTempCByIndex(0));
    if (!closing) deepSleep(SLEEP_INTERVAL_S);
    return;
  }

  thermometer.setWaitForConversion(false);
  thermometer.requestTemperatures();
}

void loop() {
  updateTemperature();
  updateTestLoop();
  updateServo();
}
