#ifndef HELIOS_MFCC_H
#define HELIOS_MFCC_H

#include <stdint.h>
#include <stddef.h>

// ============================================================
// HELIOS MFCC CONFIGURATION
// ============================================================

#define MFCC_SAMPLE_RATE       16000

#define MFCC_FRAMES            49
#define MFCC_COEFFS            40

// 25 ms frame = 400 samples
#define MFCC_FRAME_SAMPLES     400

// 10 ms hop = 160 samples
#define MFCC_HOP_SAMPLES       160

// 512-point FFT
#define MFCC_FFT_SIZE          512

// Number of mel filters
#define MFCC_MEL_FILTERS       40

// Frequency range
#define MFCC_LOW_FREQ          20.0f
#define MFCC_HIGH_FREQ         8000.0f

// ============================================================
// MFCC API
// ============================================================

void mfcc_compute_49x40(
    const int16_t* audio,
    size_t audio_samples,
    float* output
);

#endif