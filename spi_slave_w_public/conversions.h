#ifndef CONVERSIONS_H
#define CONVERSIONS_H
// Convert a double (–1..+1) to uint16_t (0..65535)
static inline uint16_t double_to_uint16(double x)
{
    if (x > 1.0)
        x = 1.0;
    if (x < -1.0)
        x = -1.0;
    return (uint16_t)((x + 1.0) * 32767.5);
}

uint8_t double_to_u8(double x)
{
    // Clamp to valid range just in case
    if (x > 1.0)
        x = 1.0;
    if (x < -1.0)
        x = -1.0;

    // Convert -1..1 to 0..255
    return (uint8_t)((x + 1.0) * 127.5);
}

int16_t double_to_int16(double x)
{
    if (x > 1.0)
        x = 1.0;
    if (x < -1.0)
        x = -1.0;

    return (int16_t)(x * 32767.0);
}
#endif