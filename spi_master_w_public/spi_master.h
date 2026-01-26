#ifndef SPI_MASTER_H
#define SPI_MASTER_H

#include "hardware/spi.h"
#include <math.h>

#define SPI_PORT spi0
/*
These GPIOs are arbitrarily chosen for this program. You can use any of the SPIO channels.
Potential show stopper: You HAVE to connect the other side of the 16 (SPIO RX) and 19 (SPIO TX) wires
to their opposites on the slave. So, let's say you are usnig the same set of pins on the slave:
MASTER | SLAVE
16    ...   19
19    ...   16
*/
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_MOSI 19

#define BUF_LEN 0x03

// Thermistor parameters (adjust based on your thermistor's datasheet)
const float R_NOMINAL = 10000.0;     // Resistance at nominal temperature (e.g., 10kΩ at 25°C)
const float T_NOMINAL = 18.3;        // Nominal temperature in Celsius
const float B_COEFFICIENT = 10050.0; // 3950.0;   // Beta coefficient
const float VCC = 3.3;               // Pico's supply voltage
const float FIXED_RESISTOR = 9000.0; // Value of the fixed resistor in the voltage divider

// -------------------------------------------------------------
// pack24() — Safely combine 3 bytes into a 24-bit unsigned int
// -------------------------------------------------------------
// Arguments:
//   b0 = least significant byte
//   b1 = middle byte
//   b2 = most significant byte
//
// Returns:
//   24-bit value stored in a uint32_t (safe in all architectures)
// -------------------------------------------------------------
static inline uint32_t pack24(uint8_t b0, uint8_t b1, uint8_t b2)
{
    return ((uint32_t)b2 << 16) |
           ((uint32_t)b1 << 8) |
           (uint32_t)b0;
}

float getTemperature(uint16_t raw_adc)
{
    uint16_t conversion = 0;
    // Function definition: calculates the sum and returns it.
    float temperature_c;
    // Convert raw ADC to voltage
    float adc_voltage = raw_adc * (VCC / 4095.0); // 4095 for 12-bit ADC
    // printf("ADC voltage: %f\n", adc_voltage);
    //  Calculate thermistor resistance using voltage divider formula
    float thermistor_resistance = FIXED_RESISTOR / ((VCC / adc_voltage) - 1);
    // Apply Steinhart-Hart equation to calculate temperature in Kelvin
    float steinhart_temp_k = 1.0 / ((1.0 / (T_NOMINAL + 273.15)) + (log(thermistor_resistance / R_NOMINAL) / B_COEFFICIENT));
    // Convert Kelvin to Celsius
    temperature_c = steinhart_temp_k - 273.15; // Calculate Centigrade.
    conversion = (int)temperature_c;
    return temperature_c;
}

const char *byte_to_binary(uint8_t v)
{
    static char out[9];
    for (int i = 0; i < 8; i++)
    {
        out[i] = (v & (1 << (7 - i))) ? '1' : '0';
    }
    out[8] = '\0';
    return out;
}

static void spi_setup()
{
    // Enable SPI 0 at 1 MHz and connect to GPIOs
    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_4pins_with_func(PIN_MISO, PIN_MOSI, PIN_SCK, PIN_CS, GPIO_FUNC_SPI));
}

static uint32_t spi_readwrite(uint8_t *tx, uint8_t *rx)
{
    // Write the output buffer to MISO, and at the same time read from MOSI.
    gpio_put(PIN_CS, 0);
    sleep_ms(2); // debounce relief
    spi_write_read_blocking(SPI_PORT, tx, rx, 3);
    gpio_put(PIN_CS, 1);
    printf("Sent: %02X %02X %02X | Received: %02X %02X %02X\n", tx[0], tx[1], tx[2], rx[0], rx[1], rx[2]);
    // Concatenate into a single 16-bit value
    return ((uint32_t)rx[2] << 16) | (rx[1] << 8) | rx[0];
}
#endif