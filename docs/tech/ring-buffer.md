# Circular Ring Buffer

A ring buffer is a fixed array whose write head wraps at the end. New samples overwrite the oldest samples, so memory remains bounded.

HELIOS stores 32,000 samples: two seconds at 16 kHz. A push advances `head` modulo the capacity and saturates `count` at the capacity. Reading the last `M` samples starts at `(head - M + N) % N`.

The buffer preserves context while the detector decides. Without it, the command onset could be lost before verification and pre-roll recovery would be impossible.

**Memory:** `2 s × 16,000 × 2 bytes = 64 KB SRAM`.

See `firmware/arduino/HELIOS/ring_buffer.h`. The intended API is `rb_init`, `rb_push`, and `rb_read_last`. A queue is unsuitable because it grows or requires a separate bound; the ring buffer provides O(1) writes and constant memory.
