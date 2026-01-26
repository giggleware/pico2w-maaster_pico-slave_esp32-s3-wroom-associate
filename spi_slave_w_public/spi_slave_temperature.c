// Copyright (c) 2021 Michael Stoops. All rights reserved.
// Portions copyright (c) 2021 Raspberry Pi (Trading) Ltd.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
// following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
//    disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//    following disclaimer in the documentation and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
//    products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// SPDX-License-Identifier: BSD-3-Clause
//
// Example of an SPI bus slave using the PL022 SPI interface

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/adc.h"
#include "spi_slave.h"
#include "trig_functions.h"

// Conversion factor for the ADC (12-bit resolution, 3.3V reference)
// Note: The actual Vref can vary, impacting accuracy.
#define CONVERSION_FACTOR 3.3f / (1 << 12)

void printbuf(uint8_t buf[], size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i)
    {
        if (i % 16 == 15)
            printf("%02x\n", buf[i]);
        else
            printf("%02x ", buf[i]);
    }

    // append trailing newline if there isn't one
    if (i % 16)
    {
        putchar('\n');
    }
}

int main()
{
    // Enable UART so we can print
    stdio_init_all();
#if !defined(SPI_PORT) || !defined(PIN_SCK) || !defined(PIN_MOSI) || !defined(PIN_MISO) || !defined(PIN_CS)
#warning spi/spi_slave example requires a board with SPI pins
    puts("Default SPI pins were not defined");
#else

    printf("SPI slave example\n");

    spi_setup();

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    // Initialize output buffer
    for (size_t i = 0; i < BUF_LEN; ++i)
    {
        // bit-inverted from i. The values should be: {0xff, 0xfe, 0xfd...}
        out_buf[i] = ~i;
    }

    printf("SPI slave says: When reading from MOSI, the following buffer will be written to MISO:\n");
    printbuf(out_buf, BUF_LEN);

    adc_init();                        // Initialize the ADC
    adc_set_temp_sensor_enabled(true); // Enable the temperature sensor
    adc_select_input(4);               // Select ADC input 4, which is connected to the temperature sensor

    for (size_t i = 0;; ++i)
    {

        // ONBOARD TEMPERATURE SENSOR
        // Read the raw 12-bit ADC value
        uint16_t raw = adc_read();
        // Convert the raw value to a voltage
        float voltage = raw * CONVERSION_FACTOR;
        // Convert the voltage to temperature in Celsius using the datasheet formula
        // T = 27 - (ADC_voltage - 0.706)/0.001721
        float celsius = 27.0f - (voltage - 0.706f) / 0.001721f; //
        float temperature = (celsius * 1.8) + 32;
        uint8_t fahrenheit = (int)(temperature);
        // Print the temperature
        printf("Temperature: %d F\n", fahrenheit);
        out_buf[0] = fahrenheit;
        uint16_t output = spi_readwrite(out_buf, in_buf);
    }
#endif
}