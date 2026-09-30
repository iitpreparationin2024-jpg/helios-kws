# Whisper Automatic Speech Recognition

Whisper is an open-source multilingual speech-to-text model that runs locally without API keys. HELIOS uses a remote server because even the small models are far beyond ESP32-S3 memory and flash limits.

| Model | Parameters | Approx. size | Relative CPU latency |
|---|---:|---:|---:|
| tiny | 39M | 75 MB | ~0.5× real time |
| base | 74M | 142 MB | ~1× |
| small | 244M | 484 MB | ~2× |

`base` favors accuracy; `tiny` favors speed. A two-second command and base CPU inference produce an approximately three-second round trip in the demo budget. Alternatives include Vosk, Coqui STT, and Faster-Whisper. The WebSocket boundary keeps the firmware ASR-agnostic.

See `server/asr_server.py` and `server/requirements.txt`.
