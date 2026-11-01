# Window Controller

ESP32 + hobby servo that opens and closes a window. It will be controlled by buttons and a thermometer; for now a DS18B20 thermometer closes the window when the temperature drops below a threshold.

## Wiring

| Servo wire        | Connect to                          |
|-------------------|-------------------------------------|
| Signal (orange/yellow) | ESP32 **GPIO 26**              |
| V+ (red)          | **External 5–6 V supply** (+)       |
| GND (brown/black) | External supply (−) **and** ESP32 GND |

Don't power the servo from the ESP32's 3.3 V pin. Under load a servo can draw 1 A or more, which resets the board (brownout). A small SG90 with no load can run from the `VIN`/`5V` pin while USB is plugged in, but anything that moves a real window needs its own supply (an MG996R with a 5 V 2–3 A adapter is a common choice). The grounds must be connected together.

| DS18B20 wire      | Connect to                          |
|-------------------|-------------------------------------|
| Data (yellow)     | ESP32 **GPIO 5**                    |
| VDD (red)         | ESP32 **3.3 V**                     |
| GND (black)       | ESP32 GND                           |

Put a **4.7 kΩ resistor between Data and 3.3 V** (pull-up). Without it the sensor reads -127 °C or isn't found. Some breakout modules already have it on board.

| Re-arm button     | Connect to                          |
|-------------------|-------------------------------------|
| One leg           | ESP32 **GPIO 4**                    |
| Other leg         | ESP32 GND                           |

No resistor needed; the ESP32's internal pull-up is used.

## Battery use

With `LOW_POWER = true` (battery mode), the board wakes every `SLEEP_INTERVAL_S` (60 s), reads the temperature once and deep-sleeps again. After closing the window it sleeps until the re-arm button is pressed. Waking up restarts the program, so the window counts as open again and will close the next time it gets cold. The CPU runs at 80 MHz and the servo gets no signal unless it's moving.

Set `LOW_POWER = false` while developing: the board stays awake and prints a reading every second (`TEMP_INTERVAL_MS`). In this mode it doesn't sleep after closing either; press the reset button to start over.

On a standard ESP32 devkit the board's regulator, USB chip and LED still draw some current during sleep, so expect roughly a week on 4×AA. A big capacitor (470–1000 µF) across the servo supply helps against resets when the servo starts.

## Build and flash (VS Code)

1. Open this folder in VS Code with the **PlatformIO IDE** extension installed.
2. Click **Upload** (→) in the PlatformIO status bar, then **Serial Monitor** (plug icon) to watch the progress messages at 115200 baud.

Or from a terminal:

```bash
~/.platformio/penv/bin/pio run -t upload -t monitor
```

## Tuning

Everything is at the top of [src/main.cpp](src/main.cpp):

The servo is an MG995 360° (continuous rotation): the signal sets speed and direction, not position, so a move is "spin for N ms".

- `STOP_US`: pulse width where the servo stands still. Nudge it by 5–10 µs if it creeps when it should be stopped.
- `SPEED_US`: how hard it drives while moving (bigger means faster, max ~500).
- `CLOSE_MS`: how long the closing move lasts. Tune it on the real window.
- `CLOSE_BELOW_C`: temperature (°C) below which the window closes.
- `LOW_POWER`: `true` for batteries (read every `SLEEP_INTERVAL_S`, sleep in between), `false` for development (stay awake, read every `TEMP_INTERVAL_MS`).
- `SLEEP_INTERVAL_S`: seconds between readings in battery mode.
- `TEMP_INTERVAL_MS`: how often the thermometer is read in development mode.
- `SERVO_PIN`, `TEMP_PIN`, `BUTTON_PIN`: signal pins. The button pin must be an RTC pin to wake the board.
- `CPU_MHZ`: CPU clock; 80 MHz saves power and is the lowest that keeps serial and servo timing reliable.
- `TEST_LOOP`: closes every `TEST_INTERVAL_MS` regardless of temperature and never sleeps. Keep it `false` for normal use.

The window is assumed to be open at boot. Once closed, the board sleeps until the re-arm button (or the reset button) is pressed.
