# Window Controller

ESP32 + hobby servo that opens and closes a window. It will be controlled by buttons and a thermometer; for now the firmware runs a sweep test that moves the servo back and forth by 90° forever.

## Wiring

| Servo wire        | Connect to                          |
|-------------------|-------------------------------------|
| Signal (orange/yellow) | ESP32 **GPIO 13**              |
| V+ (red)          | **External 5–6 V supply** (+)       |
| GND (brown/black) | External supply (−) **and** ESP32 GND |

Don't power the servo from the ESP32's 3.3 V pin. Under load a servo can draw 1 A or more, which resets the board (brownout). A small SG90 with no load can run from the `VIN`/`5V` pin while USB is plugged in, but anything that moves a real window needs its own supply (an MG996R with a 5 V 2–3 A adapter is a common choice). The grounds must be connected together.

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
- `OPEN_MS` / `CLOSE_MS`: how long each move lasts. Tune these until a move is ~90°.
- `PAUSE_AT_END_MS`: how long it waits at each end.
- `SERVO_PIN`: signal pin.
