#ifndef TRIG_FUNCTIONS_H
#define TRIG_FUNCTIONS_H

#include <math.h>

// Number of samples in a full cycle
#define WAVEFORM_SAMPLES 360

// Convert an index (0–359) to radians
static inline double idx_to_radians(int idx)
{
    // Wrap index into range
    idx %= WAVEFORM_SAMPLES;
    if (idx < 0)
        idx += WAVEFORM_SAMPLES;
    return (idx * M_PI) / 180.0;
}

// Sine from index
static inline double sine_at(int idx)
{
    return sin(idx_to_radians(idx));
}

// Cosine from index
static inline double cosine_at(int idx)
{
    return cos(idx_to_radians(idx));
}

// Tangent from index
static inline double tangent_at(int idx)
{
    return tan(idx_to_radians(idx));
}

// Cotangent from index (safe version)
static inline double cotangent_at(int idx)
{
    double t = tan(idx_to_radians(idx));
    if (fabs(t) < 1e-12)
        return HUGE_VAL; // Avoid division by zero
    return 1.0 / t;
}
#endif // TRIG_FUNCTIONS_H
