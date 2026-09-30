# Pre-Roll Recovery

The keyword detector needs time to verify `HELIOS`, but the user may already have started “turn on the light.” Without recovery, ASR can receive only the end of the command.

The ring buffer always retains two seconds. When verification opens the gate, firmware reads the last one second, prepends it to the live stream, and then continues with VAD-gated audio. One second covers the roughly 700 ms keyword and verifier latency with margin.

The operation reads from the existing 64 KB buffer; one second is 16,000 samples (32 KB) and requires no new allocation. Longer history adds irrelevant audio, while shorter history risks losing the first command syllable.

See `rb_read_last()` in `firmware/arduino/HELIOS/HELIOS.ino` and `ring_buffer.h`.
