# Temporal Verifier

The verifier prevents one uncertain frame from opening the gate. It keeps the last five inference results and opens only when at least three classify as HELIOS with confidence at least 0.60.

A real utterance produces a cluster of high-confidence frames; transient noise and partial matches usually produce isolated hits. Five windows cover roughly the keyword duration, while a majority of three tolerates one or two dropped frames. The rolling rule is more robust than a single confidence threshold.

The state uses approximately 50 bytes and takes about 1 microsecond per call. See `firmware/arduino/HELIOS/verifier.cc` and the `verifier_reset` / `verifier_push` interface.
