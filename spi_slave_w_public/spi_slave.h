#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_MOSI 19

#define BUF_LEN 0x03

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
    spi_set_slave(SPI_PORT, true);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_4pins_with_func(PIN_MISO, PIN_MOSI, PIN_SCK, PIN_CS, GPIO_FUNC_SPI));
}

static inline uint16_t spi_readwrite(uint8_t out_buf[], uint8_t in_buf[])
{
    // Write the output buffer to MISO, and at the same time read from MOSI.
    spi_write_read_blocking(SPI_PORT, out_buf, in_buf, BUF_LEN);
    // Now, rx[0] contains the MSB and rx[1] contains the LSB
    printf("Received: %02X %02X %02X | Sent: %02X %02X %02X\n", in_buf[0], in_buf[1], in_buf[2], out_buf[0], out_buf[1], out_buf[2]);
    // Concatenate into a single 16-bit value
    uint32_t outgoing_command = ((uint16_t)in_buf[0] << 16) | ((uint16_t)in_buf[1] << 8) | in_buf[2];
    printf("outgoing_command : %04X\n", outgoing_command);
    return outgoing_command;
}
#endif