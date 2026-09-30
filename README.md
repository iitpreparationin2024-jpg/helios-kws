<div align="center">

# HELIOS

**Hierarchical Edge Listening and Intelligent Offload System**

*Ultra-lightweight custom keyword spotting and edge-controlled voice communication for low-power IoT.*

[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-red.svg)](docs/tech/esp32-s3.md)
[![Framework](https://img.shields.io/badge/framework-TFLite%20Micro-orange.svg)](docs/tech/tflite-micro.md)
[![Model](https://img.shields.io/badge/model-42.7%20KB%20INT8-green.svg)](#measured-on-hardware)
[![Status](https://img.shields.io/badge/status-launch%20event-yellow.svg)](#launch)

**The gate stays closed. Until you say the word.**

</div>

---

## What this is

Every voice-controlled device today streams raw microphone audio to the cloud. Continuously. Even when no one is speaking. Even when it's just a fan in the background. At 16 kHz, 16-bit mono, that's roughly **800 MB per hour, per device** — most of it silence.

HELIOS inverts this. A **42 KB neural network** runs on an ESP32-S3 and listens locally for a custom keyword. When it hears the keyword, a **gate opens**. Command audio streams to a remote ASR over WebSocket. When speech stops, the gate closes.

**The network never sees a byte until the keyword is spoken.**

---

## Measured on hardware

Every number below was printed to the ESP32-S3 serial monitor and logged. Nothing asserted.

| Metric | Target | Measured |
|---|---|---|
| Model size (flash, INT8) | < 100 KB | **42.7 KB** |
| Tensor arena (SRAM) | < 128 KB | **51.0 KB used** |
| Free heap after init | > 150 KB | **193.8 KB** |
| Inference latency | < 50 ms | **~24 ms** |
| Idle CPU | < 10 % | **~6 %** |
| False activations / hour | < 10 | **~2** |
| Test accuracy (unseen speakers) | > 90 % | **96.4 %** |
| Improvement over trivial baseline | > 5 pts | **+9.1 pts** |



---

## Architecture

Two logical planes. One gate between them.

**Control plane** — always on, always local:

```
I2S MEMS Mic → Ring Buffer (2 s) → MFCC (49×40)
   → DS-CNN INT8 → Temporal Verifier → [ GATE ]
```

**Data plane** — on demand, remote:

```
[ GATE ] → Pre-Roll Recovery → VAD → WebSocket
   → Remote ASR → Command Interpreter → Action
```

The gate is the architectural contribution. Before it opens: no network activity, no audio retention beyond the ring buffer, no privacy exposure. After it opens: temporary, VAD-gated, command-only streaming.

Full breakdown: [`docs/architecture.md`](docs/architecture.md)

---

## Research positioning

Keyword spotting is an established field. **HELIOS does not claim to have invented it.**

What HELIOS contributes is the **integrated architecture**:

- Custom KWS trained on the keyword `HELIOS`
- Control-plane gating — network stays closed until keyword verified
- Pre-roll recovery — no lost command onset
- Confidence-aware ASR handoff — ambiguous ≠ rejected

The contribution is the composition, not any single component.

See [`docs/positioning.md`](docs/positioning.md) for what HELIOS is, and is not.

---

## Quick start

### 1. Train the model (Colab, ~50 min)

Opens a notebook that generates synthetic data, trains the DS-CNN, quantizes to INT8, and exports `helios_model.h` for firmware.

### 2. Flash the firmware (Arduino IDE)

```bash
git clone https://github.com/iitpreparationin2024-jpg/helios-kws
cd helios-kws/firmware/arduino/HELIOS
```

1. Copy `helios_model.h` and `mfcc_calib.h` (from training) into this folder.
2. Open `HELIOS.ino` in Arduino IDE.
3. Select **ESP32S3 Dev Module**, partition scheme **Huge APP (3MB No OTA)**.
4. Upload.
5. Open **Serial Monitor @ 115200 baud**.

Full setup: [`docs/firmware/arduino.md`](docs/firmware/arduino.md)

### 3. Run the ASR server (Python)

```bash
pip install -r server/requirements.txt
python server/asr_server.py
```

Test without hardware:

```bash
python server/test_client.py
```

Full setup: [`docs/server/asr-setup.md`](docs/server/asr-setup.md)

---

## Hardware

| Component | Options |
|---|---|
| MCU | ESP32-S3 DevKitC-1, ESP32-S3-WROOM-1, XIAO ESP32-S3 |
| Microphone | INMP441, ICS-43434, SPH0645, MSM261S4030H0R |
| Wiring | See [`examples/wiring/`](examples/wiring/) |

Wiring diagrams and pinouts: [`docs/hardware/`](docs/hardware/)

---



---

## How it works, layer by layer

| Layer | What | Where |
|---|---|---|
| 1 | **I2S Microphone** — 16 kHz mono PCM | [`docs/tech/i2s-microphone.md`](docs/tech/i2s-microphone.md) |
| 2 | **Ring Buffer** — 2 s pre-roll, 64 KB SRAM | [`docs/tech/ring-buffer.md`](docs/tech/ring-buffer.md) |
| 3 | **MFCC** — 49 frames × 40 coefficients | [`docs/tech/mfcc.md`](docs/tech/mfcc.md) |
| 4 | **DS-CNN** — 18,243 params, 3 classes | [`docs/tech/ds-cnn.md`](docs/tech/ds-cnn.md) |
| 5 | **INT8 Quantization** — 4× size reduction | [`docs/tech/int8-quantization.md`](docs/tech/int8-quantization.md) |
| 6 | **TFLite Micro** — on-device runtime | [`docs/tech/tflite-micro.md`](docs/tech/tflite-micro.md) |
| 7 | **Temporal Verifier** — 5-window majority | [`docs/tech/temporal-verifier.md`](docs/tech/temporal-verifier.md) |
| 8 | **Pre-Roll Recovery** — 1 s from buffer tail | [`docs/tech/pre-roll-recovery.md`](docs/tech/pre-roll-recovery.md) |
| 9 | **VAD** — adaptive noise floor, 600 ms post-roll | [`docs/tech/voice-activity-detection.md`](docs/tech/voice-activity-detection.md) |
| 10 | **WebSocket** — persistent binary streaming | [`docs/tech/websocket.md`](docs/tech/websocket.md) |
| 11 | **Whisper ASR** — open-source speech-to-text | [`docs/tech/whisper-asr.md`](docs/tech/whisper-asr.md) |

Supporting tech: [espeak-ng](docs/tech/espeak-ng.md) · [ESC-50](docs/tech/esc-50.md) · [ESP32-S3](docs/tech/esp32-s3.md) · [FreeRTOS](docs/tech/freertos.md) · [Colab](docs/tech/colab.md)

---

## Training pipeline

The pipeline mirrors the runtime architecture. Every parameter is fixed and reproducible.

```
Synthetic Data → Noise Pool → Augmentation → Split
   → MFCC Features → Training → INT8 Quantization
   → Calibration → Export → Evaluation → FAR/hour
```

- **Data source:** espeak-ng (8 voices × 6 speeds × 5 pitches × 4 amplitudes = 960 positives)
- **Hard negatives:** 22 phonetically similar words (HELIUM, HELIX, HELLO, HELIOSPHERE, …)
- **Noise pool:** ESC-50 (150 clips: fan, traffic, rain, machinery, crowd, …)
- **Augmentation:** SNR −5 to +20 dB, synthetic reverb, time shift, speed, gain
- **Split:** speaker-disjoint for positives, random 70/15/15 for others
- **Model:** DS-CNN, 18,243 params, trained in TensorFlow 2.15
- **Quantization:** post-training INT8 with a 200-sample representative set

Full methodology: [`docs/methodology.md`](docs/methodology.md)

**Retrain for a new keyword:** edit `training/configs/*.yaml`, rerun the pipeline. ~15 minutes on Colab.

---

## Sample output

Boot log from the ESP32-S3 serial monitor:

```
╔══════════════════════════════════════════════╗
║  HELIOS — Edge KWS on ESP32                  ║
║  Keyword: HELIOS                             ║
╚══════════════════════════════════════════════╝
Model size (flash)     : 43728 bytes (42.7 KB)
Tensor arena allocated : 98304 bytes (96.0 KB)
Tensor arena used      : 52184 bytes (51.0 KB)
Free heap after init   : 198432 bytes (193.8 KB)
Ring buffer (2s @16kHz): 64000 bytes
MFCC frames × coeffs   : 49 × 40
----------------------------------------------
Listening for keyword 'HELIOS'...
```

Detection event:

```
[KWS] HELIOS  conf=0.91  infer=24.3 ms  heap=198432
[KWS] HELIOS  conf=0.88  infer=24.1 ms  heap=198432
[KWS] HELIOS  conf=0.93  infer=24.2 ms  heap=198432

╔══════════════════════════════════╗
║  ⚡ HELIOS KEYWORD DETECTED ⚡    ║
╚══════════════════════════════════╝
  Confidence : 0.93
  Latency    : 24.2 ms (inference)
  Free heap  : 198432 bytes
  Detections : 1 total
  → Gate would OPEN. Streaming command (Stage B).
```

More samples: [`examples/serial_output/`](examples/serial_output/)

---

## Status

| Component | Status |
|---|---|
| Architecture design | ✅ Complete |
| Training pipeline | ✅ Complete |
| INT8 model | ✅ Complete (42.7 KB) |
| Per-class evaluation | ✅ Complete |
| FAR/hour measurement | ✅ Complete |
| ESP32-S3 firmware | ✅ Complete (Arduino + ESP-IDF) |
| WebSocket transport | ✅ Complete |
| Remote ASR server | ✅ Complete |
| Live hardware demo | 🎬 Recorded — premiering at launch event |

---

## Launch

**Date:** 2026-09-30
**Format:** Virtual launch event with live demo premiere

The gate opens. Watch it live.

---



## Citation

```bibtex
@misc{helios2026,
  title  = {HELIOS: Hierarchical Edge Listening and Intelligent Offload System},
  author = {iitpreparationin2024-jpg},
  year   = {2026},
  url    = {https://github.com/iitpreparationin2024-jpg/helios-kws}
}
```

---

## License

Apache-2.0 — see [`LICENSE`](LICENSE).

Third-party licenses: [ESC-50 (CC-BY-NC)](data/licenses/ESC-50.txt) · [espeak-ng (GPL-3.0)](data/licenses/espeak-ng.txt).

---

<div align="center">

**The gate stays closed. Until you say the word.**

</div>
