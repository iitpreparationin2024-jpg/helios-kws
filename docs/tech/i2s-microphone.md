# I2S MEMS Microphone

## What it is
I2S is a synchronous digital-audio bus. A MEMS microphone sends PCM samples over BCLK, WS/LRCLK, and SD, avoiding an analog signal path and ADC.

## HELIOS configuration
The ESP32-S3 is the bus controller. HELIOS captures 16 kHz, mono, 16-bit PCM in 30 ms frames.

| Mic | Format | Note |
|---|---|---|
| INMP441 | 24-in-32 | L/R tied low for left channel |
| ICS-43434 | 24-in-32 | Similar wiring |
| SPH0645 | 18-in-32 | Requires a different bit shift |
| MSM261S4030H0R | 24-in-32 | Industrial option |

## Wiring: INMP441 to ESP32-S3

| Mic | ESP32-S3 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SCK | GPIO 5 |
| WS | GPIO 6 |
| SD | GPIO 4 |
| L/R | GND |

## Why
Digital output is deterministic and avoids analog noise. Mono is sufficient for keyword spotting and 16 kHz matches the training representation.

## Code and gotchas
See `firmware/arduino/HELIOS/HELIOS.ino` and `firmware/esp-idf/main/main.cc`. Tie L/R; do not power the mic from 5 V; and verify the bit shift (`>>14` for common INMP441 data, typically `>>8` for SPH0645). Check GPIO conflicts when PSRAM is enabled.
