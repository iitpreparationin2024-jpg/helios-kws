# Depthwise-Separable CNN (DS-CNN)

A DS-CNN replaces a standard convolution with depthwise convolution followed by a pointwise 1×1 convolution. Standard parameters are `F×K²×C`; the separable form is `K²×C + F×C`, about `F×C×(K²/9+1)` for a 3×3 kernel.

## HELIOS model

`(49,40,1) → Conv2D(32, 3×3, stride 2) → BN/ReLU → 3 × [Depthwise 3×3 → BN/ReLU → Pointwise 1×1 → BN/ReLU → MaxPool]`, with filters 64, 64, and 128, then global average pooling, dropout 0.3, and a three-class softmax.

The model has 18,243 parameters: about 71 KB in FP32 and 42 KB in INT8. This is small enough for ESP32-S3 flash and fast enough for the target inference budget.

Training uses sparse categorical cross-entropy, Adam at 1e-3, ReduceLROnPlateau, class weights `{HELIOS: 3, UNKNOWN: 1, SILENCE: 1}`, early stopping patience 8, and up to 50 epochs. Classes are 0 HELIOS, 1 UNKNOWN, and 2 SILENCE.

See `training/scripts/train.py` and the generated runtime in `firmware/arduino/HELIOS/kws_engine.cpp`.
