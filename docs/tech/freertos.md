# FreeRTOS

FreeRTOS is the preemptive RTOS included by ESP-IDF and Arduino-ESP32. It provides tasks, queues, semaphores, timers, and scheduling; the Arduino `setup()`/`loop()` runs inside a task while Wi-Fi and I2S use system tasks and DMA.

The initial firmware deliberately keeps the control loop simple. A future implementation can pin KWS to Core 1 and WebSocket work to Core 0, using a queue to isolate inference timing from network jitter.

| Core | Work | Priority |
|---|---|---|
| 0 | Wi-Fi stack and WebSocket | system / 3 |
| 1 | Arduino loop and KWS | 1 |
| hardware | I2S DMA ISR | hardware |

Firmware references are `firmware/arduino/HELIOS/HELIOS.ino` and `firmware/esp-idf/main/main.cc`.
