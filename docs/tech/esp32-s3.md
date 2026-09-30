# ESP32-S3

The ESP32-S3 is a dual-core Xtensa LX7 MCU with Wi-Fi, Bluetooth LE, 512 KB SRAM, optional PSRAM, and INT8 vector instructions. HELIOS benefits from the vector instructions for quantized inference and from integrated Wi-Fi for WebSocket transport.

| Region | Approx. size |
|---|---:|
| Tensor arena | 96 KB |
| Ring buffer | 64 KB |
| MFCC scratch | 8 KB |
| FreeRTOS/Wi-Fi | ~150 KB |

Reference boards are ESP32-S3-DevKitC-1, ESP32-S3-WROOM-1, and XIAO ESP32-S3. A practical split is Wi-Fi on Core 0 and audio/KWS on Core 1, subject to measurement.

Arduino setup: install the Espressif board package, select **ESP32S3 Dev Module**, choose a suitable partition, and enable OPI PSRAM only when present. See `firmware/arduino/HELIOS/HELIOS.ino`.
