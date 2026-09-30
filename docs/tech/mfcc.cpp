#ifndef HELIOS_MFCC_H
#define HELIOS_MFCC_H

#include <stdint.h>
#include <stddef.h>

#define MFCC_SAMPLE_RATE       16000

#define MFCC_FRAMES            49
#define MFCC_COEFFS            40

#define MFCC_FRAME_SAMPLES     400
#define MFCC_HOP_SAMPLES       160

#define MFCC_FFT_SIZE          512
#define MFCC_MEL_FILTERS       40

#define MFCC_LOW_FREQ          20.0f
#define MFCC_HIGH_FREQ         8000.0f

void mfcc_compute_49x40(
    const int16_t* audio,
    size_t audio_samples,
    float* output
);

#endif