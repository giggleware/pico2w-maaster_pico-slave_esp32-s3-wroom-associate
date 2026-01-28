// Copyright (c) 2026 Gregory Lewis. All rights reserved.
// Portions copyright (c) 2021 Raspberry Pi (Trading) Ltd.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted.
// If you want to use my code in its present form, you really should put this copyright notice somewhere
// on your main file.
//
// Combining a master configuration over an SPI bus, with http functionality.
// Written for the Raspberry Pi Pico 2W to act as a master, making requests of the slave (a Raspberry Pi Pico).
// The Pico 2W also communicates with an unrelated microcontroller over UART, sending it the temperature
// received from the Pico slave, and whatever text messages are sent from the interactive web page.
//
// The master receives three bytes from the slave: The MSB (byte 0) and the middle byte (byte 1) contain the
// value registered by the Pico slave's ADC, which is reading an external thermistor.
// Byte #2 from the slave contains these LED states:
//  0 (00000000) = off;
//  1 (00000001) = green;
//  2 (00000020) = yellow;
//  3 (00000011) = yellow + green;
//  4 (00000100) = red;
//  5 (00000101) = red + green;
//  6 (00000110) = red + yellow;
//  7 (00000111) = red + yellow + green;
//  8 (00001000) = blue; 9 = blue + green;
//  10 (00001010) = blue + yellow;
//  11 (00001011) = blue + yellow + green;
//  12 (00001100) = blue + red;
//  13 (00001101) = blue + red + green;
//  14 (00001110) = blue + red + yellow;
//  15 (00001111) = blue + red + yellow + green.
// All binary patterns.

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "spi_master.h"
#include "wifi_master.h"
#include "http_helper.h"
#include "core_helper.h"
#include <limits.h>

#define UART_ID uart1
#define BAUD_RATE 115200

#define UART_TX_PIN 4
#define UART_RX_PIN 5

const uint ESP_READY_PIN = 3;
const uint LED_PIN = 0;

uint8_t loop_count = 0;

void print_byte_binary(unsigned char byte)
{
    // Determine the number of bits in a byte (usually 8)
    int bits = sizeof(byte) * CHAR_BIT;

    for (int i = bits - 1; i >= 0; i--)
    {
        // Create a mask with a 1 at the i-th bit position
        unsigned char mask = 1U << i;

        // Use bitwise AND to check if the i-th bit is set
        if (byte & mask)
        {
            printf("1");
        }
        else
        {
            printf("0");
        }
    }
    printf("\n"); // Add a newline character at the end
}

void handle_error(const char *module, struct pbuf *p)
{
    printf("Error sending %s\n", module);
    pbuf_free(p);
    p = NULL;
    // You can also add additional error handling here, such as closing the connection
}

// ---------------- Helper: chunked send ----------------
void tcp_send_chunks(struct tcp_pcb *pcb, const char *data)
{
    size_t len = strlen(data);
    size_t offset = 0;
    const size_t chunk_size = 512; // safe chunk for lwIP

    while (offset < len)
    {
        size_t to_send = (len - offset) > chunk_size ? chunk_size : (len - offset);
        err_t w = tcp_write(pcb, data + offset, to_send, TCP_WRITE_FLAG_COPY);
        if (w != ERR_OK)
        {
            // If write fails, try to flush and break
            tcp_output(pcb);
            break;
        }
        tcp_output(pcb);
        offset += to_send;
    }
    // ensure data flushed and close connection
    tcp_close(pcb);
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
    // --- Init stdio & networking ---
    stdio_init_all();
    multicore_launch_core1(core1_main);

#if !defined(SPI_PORT) || !defined(PIN_SCK) || \
    !defined(PIN_MOSI) || !defined(PIN_MISO) || !defined(PIN_CS)

#warning spi/spi_master example requires a board with SPI pins
    puts("Default SPI pins were not defined");

#else
    // --- SPI setup ---
    spi_setup();

    uint8_t spi_tx[3] = {0x00, 0x00, 0x00};
    uint8_t spi_rx[3] = {0x00, 0x00, 0x00};
    uint8_t tx_cmd = 0;
    uint8_t slave_cmd = 0;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(ESP_READY_PIN);
    gpio_set_dir(ESP_READY_PIN, GPIO_OUT);

    // Initialize UART
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    gpio_put(ESP_READY_PIN, 1);

    tx_cmd = 0; // toggling tx_cmd off here.

    while (true)
    {
        /*
         * Byte 2 is the command byte.
         * It is set by POST /api/control.
         * We consume it ONCE and immediately clear it.
         */

        /* Pull command ONCE */
        if (pending_cmd != 0)
        {
            slave_cmd = pending_cmd;
            pending_cmd = 0;
        }

        /* Build TX frame — NO RX FEEDBACK */
        uint8_t tx_buf[3] = {0x00, 0x00, slave_cmd};
        uint8_t rx_buf[3] = {0};

        slave_cmd = 0; // zero out so it doesn't keep sending the same command.

        /* Process RX only */
        /*
        current_temp_raw = ((uint16_t)rx_buf[1] << 8) | rx_buf[0];
        current_led_byte = rx_buf[2];
        */

        spi_tx[2] = tx_cmd;
        // tx_cmd = 0; // toggling tx_cmd off here.
        //  --- SPI transaction ---
        uint32_t spi_value = spi_readwrite(tx_buf, spi_rx);

        slave_output = ((uint16_t)spi_rx[2] << 16) | ((uint16_t)spi_rx[1] << 8) | spi_rx[0];
        printf("slave_output : %d\n", slave_output);

        // --- Decode SPI response ---
        uint16_t temp_raw =
            ((uint16_t)spi_rx[1] << 8) | spi_rx[0];

        float temp_c = getTemperature(temp_raw);
        float temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;

        // --- Update globals used by HTTP API ---
        current_temp_raw = (uint16_t)temp_f;
        current_led_byte = spi_rx[2];

        gpio_put(ESP_READY_PIN, 0);

        // Send numeric command instead of "Hello"
        char uart_buf[32];
        snprintf(uart_buf, sizeof(uart_buf), "CMD=%d %d\n", (int)temp_f, (int)temp_c);
        uart_puts(UART_ID, uart_buf);

        // Read response (non-blocking)
        while (uart_is_readable(UART_ID))
        {
            char c = uart_getc(UART_ID);
            putchar(c); // prints to USB console
        }

        printf("TX CMD = %u\n", tx_cmd);
        loop_count = 1;
        tx_cmd++;

        gpio_put(ESP_READY_PIN, 1);
        sleep_ms(10);

        if (text_pending)
        {
            char uart_buf[40];
            snprintf(uart_buf, sizeof(uart_buf), "TXT=%s\n", pending_text);
            uart_puts(UART_ID, uart_buf);
            text_pending = 0;
        }

        gpio_put(LED_PIN, 1);
        sleep_ms(20);
        gpio_put(LED_PIN, 0);

        // --- Poll interval ---
        sleep_ms(3000);
    }
#endif
}
