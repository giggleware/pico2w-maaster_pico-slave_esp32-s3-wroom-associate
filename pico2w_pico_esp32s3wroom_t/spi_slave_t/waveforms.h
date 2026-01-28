#ifndef WAVEFORMS_H
#define WAVEFORMS_H

#include <math.h>

// Number of samples
#define WAVEFORM_SAMPLES 360

// Precomputed waveforms sampled at 1-degree increments
static const double sine_wave[WAVEFORM_SAMPLES] = {
// sin(0°) to sin(359°)
#define DEG2RAD(d) ((d) * M_PI / 180.0)
#include "sine_values.inc"
};

static const double cosine_wave[WAVEFORM_SAMPLES] = {
// cos(0°) to cos(359°)
#include "cosine_values.inc"
};

static const double tangent_wave[WAVEFORM_SAMPLES] = {
// tan(0°) to tan(359°) — undefined points replaced by HUGE_VAL
#include "tangent_values.inc"
};

static const double cotangent_wave[WAVEFORM_SAMPLES] = {
// cot(θ) = 1 / tan(θ) — undefined points replaced by HUGE_VAL
#include "cotangent_values.inc"
};

#endif // WAVEFORMS_H
