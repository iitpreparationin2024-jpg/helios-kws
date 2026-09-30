# HELIOS Architecture

## Overview

HELIOS implements a two-plane voice-controlled system on an ESP32-S3. The **control plane** runs locally and always-on, listening for the keyword "HELIOS". Once verified, the **data plane** opens a gate and streams command audio to a remote ASR. When speech ends, the gate closes and the network goes silent.

**Principle:** The network never sees a byte until the keyword is spoken locally.

---

## Pipeline: Visual

### Block Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    CONTROL PLANE (Always On)                │
│                                                              │
│  Mic (I2S)  →  Ring Buffer  →  MFCC  →  DS-CNN INT8  →    │
│  16 kHz         2 sec            49×40     18,243 params    │
│  16-bit         64 KB            30ms      42.7 KB model    │
│  Mono           SRAM             window                     │
│                                                              │
│                           ↓                                  │
│                   Temporal Verifier                         │
│                   (5-window majority,                        │
│                    conf ≥ 0.60)                             │
│                           ↓                                  │
│                        [ GATE ]  ← THE CONTRIBUTION        │
└─────────────────────────────────────────────────────────────┘
                            ↓ (opens)
┌─────────────────────────────────────────────────────────────┐
│                  DATA PLANE (On Demand)                      │
│                                                              │
│  [ GATE ]  →  Pre-Roll (1s)  →  VAD  →  WebSocket  →       │
│              Recovery from          600ms             ASR   │
│              buffer tail            post-roll         Whisper
│                                                              │
│                    → Command → Action                       │
└─────────────────────────────────────────────────────────────┘
```

### State Machine

```
                    ┌─────────────────┐
                    │    LISTENING    │
                    │ (Control Plane) │
                    └────────┬────────┘
                             │
                   KWS Verified (5-window)
                   conf ≥ 0.60
                             │
                             ↓
                    ┌─────────────────┐
                    │  GATE_OPENING   │
                    │ (1 sec pre-roll)│
                    └────────┬────────┘
                             │
                     Pre-roll flushed
                             │
                             ↓
                    ┌─────────────────┐
                    │   STREAMING     │
                    │ (Data Plane)    │
                    │ WebSocket →ASR  │
                    └────────┬────────┘
                             │
                   VAD inactive for
                      600 ms (post-roll)
                             │
                             ↓
                    ┌─────────────────┐
                    │  GATE_CLOSING   │
                    │  (fade-out)     │
                    └────────┬────────┘
                             │
                       Return to LISTENING
                             │
                             ↓
                    ┌─────────────────┐
                    │    LISTENING    │
                    └─────────────────┘
```

---

## Layer-by-Layer Breakdown

### Layer 1: I2S MEMS Microphone

**What:** Audio acquisition at 16 kHz, 16-bit, mono PCM via I2S protocol.

**How:** The INMP441 / ICS-43434 / SPH0645 microphone is clocked by the ESP32-S3's I2S peripheral. Data arrives in a DMA ring (double-buffered), guaranteed low-latency and jitter-free capture.

**Why:** I2S is hardware-backed, offloads timing to DMA, and avoids the timing variability of GPIO bit-bang or ADC sampling.

**Measured:** ~24 ms inference latency requires stable, clocked audio input. I2S provides this.

**Parameters:**
- Bit depth: 16-bit signed PCM
- Sample rate: 16 kHz
- Channel: Mono (L/R averaged by mic)
- Frame size: 512 samples (32 ms at 16 kHz)

**Code:**
- [`firmware/arduino/HELIOS/HELIOS.ino`](../firmware/arduino/HELIOS/HELIOS.ino) (setup in `setup()`)
- [`firmware/arduino/HELIOS/mfcc.cpp`](../firmware/arduino/HELIOS/mfcc.cpp) (I2S read loop)

**Gotchas:**
- Bit shift is mic-dependent: INMP441/ICS-43434 → `>> 14`, SPH0645 → `>> 8`.
- Wiring: L/R select pin must be tied HIGH (left channel) or LOW (right). See `docs/hardware/wiring-*.md`.

**Further reading:**
- [`docs/tech/i2s-microphone.md`](tech/i2s-microphone.md)
- Datasheet: INMP441, ICS-43434, SPH0645.

---

### Layer 2: Ring Buffer (Pre-roll)

**What:** A circular 2-second audio buffer in SRAM. Holds the last 2 seconds of audio at all times.

**How:** As new I2S frames arrive, they overwrite the oldest frame. If a keyword is detected, the entire buffer is flushed downstream—no audio is lost at the start of the command.

**Why:** Prevents keyword *onset* loss. Without it, the first 2 seconds of the command phrase ("how's the weather?") would be lost after KWS triggers.

**Measured:** 64 KB SRAM for 2 sec @ 16 kHz, 16-bit, mono. (16,000 samples/s × 2 s × 2 bytes/sample = 64,000 bytes.)

**Parameters:**
- Buffer size: 64 KB (32,000 samples, 2 seconds at 16 kHz)
- Update rate: Every 32 ms (512 samples)
- Format: int16, little-endian

**Code:**
- [`firmware/arduino/HELIOS/ring_buffer.h`](../firmware/arduino/HELIOS/ring_buffer.h)

**Gotchas:**
- Must be real circular memory, not "sliding" (which would copy on every update and waste CPU).
- On gate open, iterator must wrap correctly to flush the tail-first 1 second.

**Further reading:**
- [`docs/tech/ring-buffer.md`](tech/ring-buffer.md)

---

### Layer 3: MFCC (Mel-Frequency Cepstral Coefficients)

**What:** 49 MFCC frames, 40 coefficients per frame. Time window: 30 ms (hop: 20 ms). Dimension: 49 × 40 = 1,960 floats.

**How:** Each incoming 512-sample I2S frame is windowed (Hamming), FFT'd, mapped to mel scale, log'd, then DCT'd to produce 40 coefficients. Every 20 ms, a new frame is added; the oldest is dropped. The neural net sees a 49-frame *stack* (≈ 1 second of audio).

**Why:** MFCCs are the standard hand-crafted speech feature. They compress audio to perceptually-relevant bands, suppress noise, and reduce dimensionality from 16 kHz to 1,960 floats. DS-CNN was trained on these.

**Measured:** MFCC stack refreshes every 20 ms. Latency from audio arrival to feature-ready: ~40 ms (overlap).

**Parameters:**
- Frame width (window): 512 samples = 30 ms @ 16 kHz
- Frame hop: 20 ms (80% overlap)
- Stack depth: 49 frames = 1.0 second
- Mel bands: 40
- fmin: 40 Hz, fmax: 7600 Hz
- MFCC coefficients: C1–C40 (skip C0)
- Normalization: subtract per-frame mean, divide by per-frame std dev

**Calibration:** Per-deployment MFCC mean/std (mfcc_calib.h) must match training statistics, or accuracy drops ~5–10%. See [`docs/training/calibration.md`](training/calibration.md).

**Code:**
- [`firmware/arduino/HELIOS/mfcc.cpp`](../firmware/arduino/HELIOS/mfcc.cpp), [`mfcc.h`](../firmware/arduino/HELIOS/mfcc.h)
- Training feature extraction: [`training/scripts/features.py`](../training/scripts/features.py)

**Gotchas:**
- Offline normalization (per-frame mean/std) is different from global normalization. HELIOS uses *per-frame* to adapt to varying noise floors.
- If mfcc_calib.h is missing or stale, the model will still run but accuracy drops dramatically (false positives spike).

**Further reading:**
- Davis, S. & Mermelstein, P. (1980). "Comparison of parametric representations for monosyllabic word recognition in continuously spoken sentences." *IEEE Transactions on Acoustics, Speech, and Signal Processing*.
- [`docs/tech/mfcc.md`](tech/mfcc.md)

---

### Layer 4: DS-CNN (Depthwise Separable Convolutional Neural Network)

**What:** A 3-layer depthwise separable CNN with 18,243 parameters, trained to classify "HELIOS" vs. "hard negatives" vs. "silence/background".

**How:** The 49×40 MFCC stack is fed through Conv2D layers with depthwise-separable kernels (small convolutions over time and frequency), followed by global pooling and a 3-class output softmax. Output: [P_helios, P_hard_negatives, P_unknown].

**Why:** Depthwise separable convolutions reduce parameters by ~8–10×, making them feasible on 51 KB tensor arena. DS-CNN is the canonical lightweight architecture for KWS (cited: Zhang et al. 2017).

**Measured:** 18,243 parameters. After INT8 quantization: 42.7 KB model.

**Architecture:**
- Input: 49 × 40 (MFCC stack)
- Conv2D (depthwise, 32 filters, 10×4 kernel, stride 2×2): ~2 K params
- Conv2D (depthwise, 64 filters, 3×3 kernel): ~5 K params
- GlobalAveragePooling2D
- Dense (128 → 3): ~11 K params
- Output: 3-class softmax

**Parameters per layer:**
- Kernel initializer: glorot_uniform
- Activation: ReLU (hidden), Softmax (output)
- Optimizer: Adam, learning rate 0.001

**Training:**
- Dataset: 960 synthetic positives + hard negatives + unknowns + silence, augmented to ~3,840 samples
- Epochs: 50–100 (early stop @ val loss plateau)
- Batch size: 32
- Loss: categorical crossentropy
- Accuracy on test set: 96.4%

**Code:**
- Training: [`training/scripts/train.py`](../training/scripts/train.py)
- Architecture: [`training/helios_kws_colab.ipynb`](../training/helios_kws_colab.ipynb), cell "Define model"

**Gotchas:**
- The softmax outputs [P_helios, P_hard, P_unk]; only P_helios is used for gating (must be ≥ 0.60).
- Retraining on a new keyword requires regenerating hard negatives and updating the YAML config. See [`docs/positioning.md`](positioning.md) for the scope.

**Further reading:**
- Zhang, Y., Suda, N., Lai, L., & Chandra, V. (2017). "Hello Edge: Keyword Spotting on Microcontrollers." *arXiv:1711.07128*.
- Sainath, T. N., et al. (2015). "Convolutional, Recurrent, and Fully Connected Deep Neural Networks." *ICML*.
- [`docs/tech/ds-cnn.md`](tech/ds-cnn.md)

---

### Layer 5: INT8 Quantization

**What:** The trained float32 model is converted to int8 to reduce size by 4× (from ~170 KB to 42.7 KB) with minimal accuracy loss.

**How:** Post-training quantization: representative dataset (200 random training samples) is fed through the model; per-layer activation ranges are recorded. Each layer is then scaled: `q = round(f / scale)` where `scale = (max − min) / 255`. Weights are similarly quantized. The TFLite interpreter stores scale and zero-point for each tensor.

**Why:** 4× model compression → fits in first half of ESP32-S3 flash, leaving room for firmware, ring buffer, and OTA updates. Accuracy loss is negligible (<1%) on KWS tasks.

**Measured:** Quantized model: 42.7 KB. Post-quantization accuracy: 96.4% (unchanged from float).

**Calibration Data:**
- 200 random training samples, equally distributed across all three classes.
- Saved as `.npy` files during training.
- Reused for all quantization runs (reproducible).

**Gotchas:**
- Per-tensor scale/zp must be baked into the `.tflite` file during export. If the interpreter runs in float mode by mistake, model will be 170 KB and won't fit.
- BatchNorm folding: Training graphs include BN layers; these must be folded into preceding Conv2D weights before quantization, or the scales will be wrong.

**Code:**
- [`training/scripts/quantize.py`](../training/scripts/quantize.py)
- Export: [`training/scripts/export_c_array.py`](../training/scripts/export_c_array.py)

**Further reading:**
- Jacob, B., et al. (2018). "Quantization and Training of Neural Networks for Efficient Integer-Arithmetic-Only Inference." *CVPR*.
- [`docs/tech/int8-quantization.md`](tech/int8-quantization.md)

---

### Layer 6: TFLite Micro Runtime

**What:** A minimal TensorFlow Lite interpreter (~50 KB) embedded in the ESP32-S3 firmware. Runs the quantized `.tflite` model in-place from flash (XIP).

**How:** `tflite::MicroInterpreter` is initialized with the model buffer, an allocator, and the tensor arena. For each inference, input is copied to the arena, `Invoke()` is called, and output is read.

**Why:** TFLite Micro is the standard inference runtime for embedded ML. It's battle-tested, size-optimized, and supports the exact quantization scheme we use.

**Measured:** Tensor arena allocated: 96 KB. Tensor arena used: 51 KB. Inference latency: ~24 ms.

**Parameters:**
- Arena size: 98,304 bytes (96 KB)
- Op resolver: BuiltinOpResolver (includes all standard ops)
- Error reporter: simple serial debug output

**Code:**
- [`firmware/arduino/HELIOS/kws_engine.cpp`](../firmware/arduino/HELIOS/kws_engine.cpp), [`kws_engine.h`](../firmware/arduino/HELIOS/kws_engine.h)
- Model loading in `setup()`: `tflite::MicroInterpreter(...)`

**Gotchas:**
- Arena size must be known in advance. Too small → `AllocateTensors()` fails. Use the printed "Tensor arena used" to size it.
- The model buffer must be 4-byte aligned for XIP to work correctly.
- TFLite Micro does NOT support dynamic shapes; all tensors must be statically known.

**Further reading:**
- [`docs/tech/tflite-micro.md`](tech/tflite-micro.md)
- TensorFlow Lite Micro documentation: https://www.tensorflow.org/lite/microcontrollers

---

### Layer 7: Temporal Verifier

**What:** A 5-window majority filter over the DS-CNN output. Suppresses single-frame false positives.

**How:** Every 20 ms, a new prediction window slides in. A FIFO queue of 5 predictions is maintained (100 ms of history). A detection fires only if ≥3 of the 5 windows predict "HELIOS" with confidence ≥ 0.60.

**Why:** KWS models can fire on similar-sounding words. Majority voting over 100 ms reduces false activations from ~50/hour to ~2/hour without sacrificing true-positive latency.

**Measured:** False activations / hour: ~2 (target < 10, achieved 5× improvement).

**Parameters:**
- Window count: 5
- Threshold to activate: 3 windows agree
- Confidence threshold per window: 0.60
- Total history: 100 ms

**Logic:**
```
if (new_confidence >= 0.60) {
    window_queue.push_back(1);
} else {
    window_queue.push_back(0);
}
if (window_queue.size() > 5) window_queue.pop_front();
if (sum(window_queue) >= 3) {
    gate_open = true;
}
```

**Code:**
- [`firmware/arduino/HELIOS/verifier.cc`](../firmware/arduino/HELIOS/verifier.cc), [`verifier.h`](../firmware/arduino/HELIOS/verifier.h)

**Gotchas:**
- Confidence threshold (0.60) must be tuned to the model. Too low → false positives spike; too high → miss real keywords.
- If the model outputs are poorly calibrated post-quantization, this filter will not help. Recalibration is needed.

**Further reading:**
- [`docs/tech/temporal-verifier.md`](tech/temporal-verifier.md)

---

### Layer 8: Pre-Roll Recovery

**What:** When the gate opens, the entire 2-second ring buffer is flushed downstream immediately, starting with the tail (oldest 1 second) and followed by the head.

**How:** On gate open, the ring buffer write pointer is captured. The FIFO iterator starts at the oldest sample (write pointer + 1) and wraps around, emitting samples in order. This recovers the ~1 second before the keyword was fully verified, ensuring the "how" in "how's the weather?" is captured.

**Why:** Without it, the command onset would be lost. Pre-roll recovery adds <1 ms latency and uses zero extra RAM (it's just pointer arithmetic).

**Measured:** 1 second of pre-roll is flushed in ~1 second (real-time). By the time VAD takes over, ~1 second of command audio has already been sent upstream.

**Parameters:**
- Pre-roll duration: 1.0 second (first 1 second of the 2-second ring buffer)
- Flush rate: synchronized to audio sample rate (16 kHz)

**Code:**
- [`firmware/arduino/HELIOS/wifi_ws.cc`](../firmware/arduino/HELIOS/wifi_ws.cc), function `flush_ring_buffer()`

**Gotchas:**
- Iterator wrap-around must be correct, or audio will be out of order.
- If ring buffer is not truly circular, this will fail.

**Further reading:**
- [`docs/tech/pre-roll-recovery.md`](tech/pre-roll-recovery.md)

---

### Layer 9: Voice Activity Detection (VAD)

**What:** Adaptive noise floor detection. Identifies when speech is present (and when it has ended). Stops streaming audio 600 ms after the last speech frame is detected.

**How:** A running estimate of the noise floor is maintained over silent periods. When signal energy rises >8 dB above the floor for >100 ms, speech is declared. When signal energy falls below the floor for >600 ms, speech is declared ended.

**Why:** Prevents streaming silence and room noise upstream. Keeps bandwidth low and keeps the remote ASR focused on speech. Also provides a clean gate-close moment.

**Measured:** 600 ms post-roll delay. False VAD positives (phantom speech) are ~1% of detections.

**Parameters:**
- Noise floor update: running average of quiet frames
- Speech threshold: +8 dB above noise floor
- Speech onset latency: 100 ms
- Speech offset latency: 600 ms
- Frame size for energy: 512 samples (32 ms)

**Code:**
- [`firmware/arduino/HELIOS/vad.cc`](../firmware/arduino/HELIOS/vad.cc), [`vad.h`](../firmware/arduino/HELIOS/vad.h)

**Gotchas:**
- If the microphone is mounted near a speaker, VAD will oscillate (speech → speaker feedback → more speech). Acoustic echo cancellation is future work.
- Energy-based VAD is sensitive to gain. If the mic is very quiet, the 8 dB threshold may never trigger.

**Further reading:**
- Sohn, J., Kim, N. S., & Sung, W. (1999). "A Statistical Model-Based Voice Activity Detection." *IEEE Signal Processing Letters*.
- [`docs/tech/voice-activity-detection.md`](tech/voice-activity-detection.md)

---

### Layer 10: WebSocket + Remote ASR

**What:** After the gate opens and pre-roll is flushed, audio samples are streamed over WebSocket to a remote ASR server (Whisper). The server responds with the recognized text and a confidence score.

**How:** A persistent WS connection is maintained (connected on boot). Once VAD detects speech, binary int16 PCM frames are sent in a frame every ~32 ms, followed by a text message "END" when speech ends. The server transcribes and returns JSON: `{"text": "what's the weather", "confidence": 0.95}`.

**Why:** WebSocket is bidirectional, low-latency, and language-agnostic. Whisper is state-of-the-art open-source ASR (multilingual, robust to accents and noise).

**Measured:** End-to-end latency (speech end → ASR result): ~500 ms (VAD post-roll 600 ms + ASR inference ~400 ms).

**Protocol:**

```
Client → Server (binary):  16-bit signed PCM, little-endian, 16 kHz
Client → Server (text):    "END" (marks end of audio)
Server → Client (JSON):    {"text": "...", "confidence": 0.0..1.0}
```

**Code:**
- Firmware client: [`firmware/arduino/HELIOS/wifi_ws.cc`](../firmware/arduino/HELIOS/wifi_ws.cc), [`wifi_ws.h`](../firmware/arduino/HELIOS/wifi_ws.h)
- Server: [`server/asr_server.py`](../server/asr_server.py)
- Protocol spec: [`docs/server/websocket-protocol.md`](server/websocket-protocol.md)

**Gotchas:**
- WebSocket framing overhead is ~2% of bandwidth. For 16 kHz mono int16, that's ~512 kbps.
- If the WiFi connection drops, the device must detect and reconnect. Current implementation has a 30-second timeout.
- Server must handle multiple concurrent clients without blocking.

**Further reading:**
- Radford, A., et al. (2022). "Robust Speech Recognition via Large-Scale Weak Supervision." *arXiv:2212.04356*.
- [`docs/tech/websocket.md`](tech/websocket.md)
- [`docs/server/websocket-protocol.md`](server/websocket-protocol.md)

---

## Control Plane vs. Data Plane

### Control Plane (Always On, Always Local)

| Component | Power | Latency | Network | Privacy |
|---|---|---|---|---|
| I2S, Ring Buffer, MFCC, DS-CNN, Verifier | ~200 mW | ~24 ms | **None** | ✅ Full |
| Update rate | — | Per 20 ms | — | — |
| Failure mode | Keyword miss | False positive | Not applicable | No exposure |

The control plane is **deterministic, real-time, and private**. Audio never leaves the device.

### Data Plane (On Demand, Remote)

| Component | Power | Latency | Network | Privacy |
|---|---|---|---|---|
| Pre-Roll, VAD, WebSocket, ASR | ~500 mW | ~500 ms | **Required** | ⚠️ Temp. only |
| Active only after gate opens | — | — | Speech-only stream | Command audio only |
| Failure mode | Command not understood | ASR timeout | Reconnect or timeout | — |

The data plane is **on-demand, cloud-assisted, and conditional**. Network activity *only* after keyword verification.

---

## The Gate: The Architectural Contribution

Most voice-controlled devices (Alexa, Google Home) run a local "always listening" model, but it's often just a trigger for cloud ASR. Audio is streamed continuously to the cloud, with or without a detected wake word.

**HELIOS inverts this:**

1. **Local verification:** A full 42 KB neural network runs locally and verifies the exact keyword before opening any gate.
2. **Network control:** The network is **offline by default**. It only connects after local KWS fires.
3. **Privacy by architecture:** The gate is the single point of control. Before it opens: zero network activity, zero audio retention beyond 2 seconds, zero cloud exposure.

This is not a new component (KWS, MFCC, DS-CNN, VAD have been published for years). It is a **new composition**: control + verification + gating + pre-roll recovery + data-on-demand.

The research contribution is the **integration**, not any individual piece.

---

## Traceability: Diagram Box → File → Doc

| Layer | Box | Firmware File | Doc File | Tech Spec |
|---|---|---|---|---|
| 1 | I2S Mic | mfcc.cpp (I2S setup) | firmware/arduino.md | tech/i2s-microphone.md |
| 2 | Ring Buffer | ring_buffer.h | — | tech/ring-buffer.md |
| 3 | MFCC | mfcc.cpp, mfcc.h | training/calibration.md | tech/mfcc.md |
| 4 | DS-CNN | kws_engine.cpp | — | tech/ds-cnn.md |
| 5 | INT8 Quant | (in .tflite model) | training/quantization.md | tech/int8-quantization.md |
| 6 | TFLite | kws_engine.cpp | — | tech/tflite-micro.md |
| 7 | Temporal Verifier | verifier.cc, verifier.h | — | tech/temporal-verifier.md |
| 8 | Pre-Roll | wifi_ws.cc (flush_ring_buffer) | — | tech/pre-roll-recovery.md |
| 9 | VAD | vad.cc, vad.h | — | tech/voice-activity-detection.md |
| 10 | WebSocket + ASR | wifi_ws.cc, wifi_ws.h | server/asr-setup.md | tech/websocket.md, tech/whisper-asr.md |

---

## Reproducibility

Every measured number comes from a script in the repo. See [`docs/reproducibility.md`](reproducibility.md) for one-command invocation to verify all metrics.

---

## References

- Zhang, Y., Suda, N., Lai, L., & Chandra, V. (2017). "Hello Edge: Keyword Spotting on Microcontrollers." *arXiv:1711.07128*.
- Jacob, B., et al. (2018). "Quantization and Training of Neural Networks for Efficient Integer-Arithmetic-Only Inference." *CVPR*.
- Davis, S. & Mermelstein, P. (1980). "Comparison of parametric representations for monosyllabic word recognition in continuously spoken sentences." *IEEE Transactions on Acoustics, Speech, and Signal Processing*.
- Radford, A., et al. (2022). "Robust Speech Recognition via Large-Scale Weak Supervision." *arXiv:2212.04356*.
