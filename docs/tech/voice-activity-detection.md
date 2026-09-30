# Voice Activity Detection (VAD)

HELIOS uses energy-based VAD. For each 30 ms frame, calculate RMS; speech is present when it exceeds the adaptive noise floor by 8 dB. Non-speech frames update the floor with α = 0.98.

Streaming continues for a 600 ms post-roll after the last speech frame so final consonants are not clipped. This reduces bandwidth and privacy exposure while allowing ASR to start promptly.

An ML VAD would cost roughly 20–40 KB flash and additional inference time. The energy method is a few microseconds per frame and is sufficient for the initial system; noisy environments may motivate a future ML VAD.

See `firmware/arduino/HELIOS/vad.cc` (`vad_init` and `vad_is_speech`). Initialize the floor carefully; continuous loud noise can mask quiet speech.
