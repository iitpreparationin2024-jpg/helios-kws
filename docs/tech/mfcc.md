# Mel-Frequency Cepstral Coefficients (MFCC)

MFCCs summarize the spectral envelope of a short audio frame on a mel frequency scale.

## Steps

1. Take 480 samples (30 ms at 16 kHz).
2. Apply a Hann window.
3. Compute a 512-point FFT and power spectrum.
4. Apply 40 triangular mel filters from 20 to 7600 Hz.
5. Take logarithms.
6. Apply a DCT-II and retain 40 coefficients.

| Parameter | Value |
|---|---|
| `n_mfcc` | 40 |
| `n_fft` | 512 |
| `win_length` | 480 |
| `hop_length` | 320 |
| `n_mels` | 40 |
| `fmin` / `fmax` | 20 / 7600 Hz |
| window / center | Hann / false |

An inference window contains 49 × 40 features. The mel scale approximates auditory resolution; the DCT decorrelates features for the classifier.

The training and device implementations must be numerically identical: window, filterbank, DCT, and normalization. Global training mean/std are baked into `mfcc_calib.h`; there is no per-utterance normalization.

Training code belongs in `training/scripts/features.py`; device code belongs in `firmware/arduino/HELIOS/mfcc.cpp` and `.h`. A radix-2 FFT is the initial implementation.
