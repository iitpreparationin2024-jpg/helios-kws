# HELIOS — ESP32 DevKitC-1 + PC microphone

This project adapts the supplied HELIOS int8 TFLite model to receive 16 kHz mono PCM over Wi-Fi instead of an I2S microphone.

## Important
The supplied ZIP contained only `helios_model.h`, `mfcc_calib.h`, and `helios_int8.tflite`. It did **not** contain the original MFCC implementation or the earlier `kws_engine.cpp`/`verifier` sources referenced by the instructions. Therefore `mfcc.cpp` here is a reconstruction of the documented 49×40 MFCC pipeline, not a claim that it is byte-for-byte identical to training preprocessing. If detection is poor, the next thing to verify is the original training MFCC settings.

## Arduino libraries
Install:
- ESP32 by Espressif Systems (3.x)
- TensorFlowLite_ESP32
- WebSockets by Markus Sattler

Board: ESP32 Dev Module (for classic ESP32 DevKitC-1 / WROOM-32). Serial: 115200.

## Arduino project
Open `firmware/HELIOS_PC_MIC.ino` in Arduino IDE. Keep all files in the same sketch folder:

HELIOS_PC_MIC/
- HELIOS_PC_MIC.ino
- helios_model.h
- mfcc_calib.h
- mfcc.h
- mfcc.cpp

Edit WIFI_SSID and WIFI_PASS in the .ino, then compile/upload.

## PC microphone
From `pc_mic`:

    python -m pip install -r requirements.txt
    python mic_sender.py --esp32 192.168.1.42

Replace the IP with the one printed by the ESP32 Serial Monitor.

The PC microphone audio is converted to little-endian signed 16-bit mono PCM at 16 kHz and sent to the ESP32 in WebSocket binary frames.

## Phone microphone
A phone browser cannot reliably request microphone permission from an ordinary insecure `http://ESP32-IP` page on modern browsers. For the first working demo, use the PC sender above. A phone front-end can be added later using HTTPS or a native sender while keeping the ESP32 WebSocket protocol unchanged.

## Expected Serial output
You should first see:

[WiFi] Connected. ESP32 IP=...
[WS] Listening on ws://...:8765
[READY] Start the PC microphone sender now.

Then, after starting `mic_sender.py`:

[WS] Client 0 connected
[KWS] SILENCE ...
[KWS] UNKNOWN ...
...
[KWS] HELIOS ...
========================================
       HELIOS KEYWORD DETECTED
========================================

## If AllocateTensors fails
Increase `ARENA_SIZE` from 160*1024 to 192*1024 or 224*1024. The model itself is only 36,176 bytes; the tensor arena is runtime RAM.


## Fixed build notes

This version is for the classic ESP32 / ESP32 Dev Module, not ESP32-S3.

The firmware uses:
- `tflite::AllOpsResolver` (compatible with the installed TensorFlowLite_ESP32 API)
- `MicroInterpreter(..., nullptr)` because this installed API does not provide `MicroErrorReporter`
- `WebSocketsServer ws(8765)` followed by `ws.begin()`
- 96 KB tensor arena to avoid the previous classic-ESP32 DRAM overflow

If `AllocateTensors()` fails, change only:
`constexpr size_t ARENA_SIZE = 96 * 1024;`
to:
`constexpr size_t ARENA_SIZE = 128 * 1024;`

Do not select an ESP32-S3 board. Select **ESP32 Dev Module**.


### Memory-fixed revision

This revision removes the persistent 40x241 float MFCC filter-bank table
(~38.6 KB DRAM). The filter response is calculated on demand. The rolling
500 ms PCM buffer is heap allocated after TFLite initialization instead of
being part of `.dram0.bss`.

The TFLite tensor arena remains 96 KB.
