#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "spi_slave.h"
#include "conversions.h"
#include "led_helper.h"
#include "lcd_helper.h"
#include "thermistor_helper.h"
#include "pico/binary_info.h"

const uint LED_PIN = 25;

// Push button
const uint BUTTON_PIN = 15; // GPIO pin connected to the button

void doDisplay(uint8_t t, char units)
{
    char buf1[18], buf2[18];
    lcd_set_cursor(0, 0);
    snprintf(buf1, 18, "Temp: %d%c", t, units);
    lcd_string(buf1);
    lcd_set_cursor(1, 0);
    snprintf(buf2, 18, "R:%d Y:%d G:%d B:%d", gpio_get(LED_R), gpio_get(LED_Y), gpio_get(LED_G), gpio_get(LED_B));
    lcd_string(buf2);
    printf("OLED:\n %s\n%s\n", buf1, buf2);
}

int initOLED()
{
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(PICO_I2C_INSTANCE, 100 * 1000); // test
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_I2C_SDA_PIN, PICO_I2C_SCL_PIN, GPIO_FUNC_I2C));

    lcd_init();
}

int programDeclarations()
{
    bi_decl(bi_program_description("Core1 reads and reports thermistor state to Core0. Core0 outputs to LEDs, 16X2 OLED, and terminal."));
    bi_decl(bi_1pin_with_name(LED_R, "LED_R"));
    bi_decl(bi_1pin_with_name(LED_Y, "LED_Y"));
    bi_decl(bi_1pin_with_name(LED_G, "LED_G"));
    bi_decl(bi_1pin_with_name(LED_B, "LED_B"));
    bi_decl(bi_1pin_with_name(LED_EXT, "LED_EXT"));

    bi_decl(bi_1pin_with_name(PICO_I2C_SDA_PIN, "PICO_I2C_SDA_PIN"));
    bi_decl(bi_1pin_with_name(PICO_I2C_SCL_PIN, "PICO_I2C_SCL_PIN"));
}

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

static inline uint8_t pack_led_states(bool red, bool yellow, bool green, bool relay)
{
    return (green << 0) | (yellow << 1) | (red << 2) | (relay << 3);
}

void core1_main()
{
    // This process operates on Core #2, (Core1). It is in charge of sensing and reporting the thermistor state.
    // The thermistor is on an ADC channel.
    adc_init();
    adc_gpio_init(26);   // Initialize ADC on GP26 (or your chosen pin)
    adc_select_input(0); // Select ADC channel 0 (corresponds to GP26)

    while (1)
    {
        // Read raw ADC value
        uint16_t raw_adc = adc_read(); // The command that reads the thermistor.
        multicore_fifo_push_blocking(raw_adc);
        sleep_ms(10);
    }
}

int main()
{
    // initOLED();
    stdio_init_all();
    initLEDs();
    zeroLEDs();
    sleep_ms(2000);
    char buf[16];

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN); // Use pull-up if button connects to ground when pressed

    multicore_launch_core1(core1_main);

    programDeclarations();

#if !defined(PICO_I2C_INSTANCE) || !defined(PICO_I2C_SDA_PIN) || !defined(PICO_I2C_SCL_PIN) || !defined(SPI_PORT) || !defined(PIN_SCK) || !defined(PIN_MOSI) || !defined(PIN_MISO) || !defined(PIN_CS)
#warning i2c/lcd_1602_i2c example requires a board with I2C pins
#else

    spi_setup();

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN], last_received_buf[BUF_LEN];
    uint8_t old_val = 0;
    uint8_t color = -1;

    // Initialize output buffer
    for (size_t i = 0; i < BUF_LEN; ++i)
    {
        // bit-inverted from i. The values should be: {0xff, 0xfe, 0xfd...}
        out_buf[i] = ~i;
    }

    uint16_t idx = 0;
    uint8_t p = 1;

    // Set up SPI slave
    spi_set_slave(SPI_PORT, true);

    uint8_t led_t = 0;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(LED_EXT);
    gpio_set_dir(LED_EXT, GPIO_OUT);

    while (true)
    {

        uint16_t raw_data = multicore_fifo_pop_blocking();
        uint16_t C = getTemperature(raw_data);
        uint16_t F = (C * (9 / 5) + 32); // Converts C to F.

        bool red_state = get_led_state(LED_R);
        bool yellow_state = get_led_state(LED_Y);
        bool green_state = get_led_state(LED_G);
        bool relay_state = get_led_state(LED_B);

        // Pack 16-bit F
        out_buf[0] = (uint8_t)(raw_data & 0xFF);        // LSB
        out_buf[1] = (uint8_t)((raw_data >> 8) & 0xFF); // MSB
        out_buf[2] = pack_led_states(red_state, yellow_state, green_state, relay_state);

        // Perform SPI full-duplex transfer
        uint32_t received_value = spi_readwrite(out_buf, in_buf);
        printf("received_value : %06X\n", received_value);
        uint8_t received_data_buf[3];
        received_data_buf[0] = (received_value >> 16) & 0xFF;
        received_data_buf[1] = (received_value >> 8) & 0xFF;
        received_data_buf[2] = received_value;

        if (received_data_buf[2] != 0 && received_data_buf[2] != old_val)
        {
            switch (received_data_buf[2])
            {
            case 1:
                color = LED_G;
                toggleLED(color, 1);
                break;
            case 2:
                color = LED_Y;
                toggleLED(color, 2);
                break;
            case 3:
                color = LED_R;
                toggleLED(color, 3);
                break;
            case 4:
                color = LED_B;
                toggleLED(color, 4);
                break;
            default:
                break; // no action
            }
        }

        old_val = received_data_buf[2];

        gpio_put(LED_PIN, 1);
        gpio_put(LED_EXT, 1);
        sleep_ms(20);
        gpio_put(LED_PIN, 0);
        gpio_put(LED_EXT, 0);
        sleep_ms(20);

        sleep_ms(100); // The slave polls every 10th of a second. It is like a little dog always
        // checking to see if its master has a bone for it.
        // lcd_clear();
    }
#endif
}