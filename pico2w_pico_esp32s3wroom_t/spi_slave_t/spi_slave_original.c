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
#include "math.h"
#include "conversions.h"

// Conversion factor for the ADC (12-bit resolution, 3.3V reference)
// Note: The actual Vref can vary, impacting accuracy.
#define CONVERSION_FACTOR 3.3f / (1 << 12)

// Store the value in a two-byte array (LSB at [0], MSB at [1])
static inline void trig_to_bytes(double trigVal, uint8_t out_buf[2])
{
    uint16_t val16 = double_to_uint16(trigVal);
    out_buf[0] = val16 & 0xFF;        // LSB
    out_buf[1] = (val16 >> 8) & 0xFF; // MSB
}

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

    spi_setup();

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    // Initialize output buffer
    for (size_t i = 0; i < BUF_LEN; ++i)
    {
        // bit-inverted from i. The values should be: {0xff, 0xfe, 0xfd...}
        out_buf[i] = ~i;
    }

    adc_init();                        // Initialize the ADC
    adc_set_temp_sensor_enabled(true); // Enable the temperature sensor
    adc_select_input(4);               // Select ADC input 4, which is connected to the temperature sensor

    uint16_t moment = 0x00;
    uint16_t idx = 0;
    uint8_t p = 1;

    // Set up SPI slave
    spi_set_slave(SPI_PORT, true);

    for (size_t i = 0;; ++i)
    {
        // --- Read from ADC ---
        uint16_t adc_raw_value = adc_read();               // Blocking read
        double voltage = adc_raw_value * 3.3f / (1 << 12); // Convert to voltage (12-bit resolution)
        trig_to_bytes(voltage, out_buf);
        printf("Raw ADC: %04x ...ADC Voltage: %.2f V\n", adc_raw_value, voltage);
        uint16_t output = spi_readwrite(out_buf, in_buf);
    }
#endif
}