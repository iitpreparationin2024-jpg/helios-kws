# INT8 Quantization

Quantization maps a float value to an integer using:

`float ≈ (int8 - zero_point) × scale`

For a range `[min,max]`, `scale ≈ (max-min)/255` and the zero point represents float zero.

HELIOS uses post-training INT8 conversion: train in FP32, calibrate with 200 representative windows, record activation ranges, then emit an integer-only TFLite model. FP32 is approximately 71 KB; INT8 is approximately 42 KB and is faster and lower power on ESP32-S3 vector instructions.

The current model documentation records input scale approximately 0.012 with zero point 0 and output scale approximately 0.0039 with zero point -128. These values must be read from the generated model rather than assumed in production code.

Calibration data must match real MFCC inputs. BatchNorm should be folded into preceding convolutions. See `training/scripts/quantize.py` and `firmware/arduino/HELIOS/kws_engine.cpp`.
