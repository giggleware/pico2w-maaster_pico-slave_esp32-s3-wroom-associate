#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "i2c_lcd.h"
#include "esp_log.h"

/* ===================== CONFIG ===================== */

#define UART_PORT UART_NUM_1
#define UART_BAUDRATE 115200
#define UART_TX_PIN 17
#define UART_RX_PIN 18

#define BUF_SIZE 256

#define SLAVE_IN 4
#define SLAVE_OUT 5
#define CLOCK_OUT 6
#define MASTER_IN 7
#define MASTER_LOOP 9

#define UART_NOTIFY_BIT 0x01

static const char *TAG = "ESP32";

/* ===================== MESSAGE TYPES ===================== */

typedef enum
{
    MSG_CMD,
    MSG_TEXT
} msg_type_t;

typedef struct
{
    msg_type_t type;
    int tf;
    int tc;
    char text[33]; // 16x2 LCD max
} worker_msg_t;

/* ===================== GLOBALS ===================== */

static TaskHandle_t uart_rx_task_handle = NULL;
static QueueHandle_t worker_queue;

/* ===================== ISR ===================== */

static void IRAM_ATTR master_in_isr_handler(void *arg)
{
    if (!uart_rx_task_handle)
        return;

    BaseType_t hpw = pdFALSE;
    xTaskNotifyFromISR(uart_rx_task_handle, UART_NOTIFY_BIT, eSetBits, &hpw);
    if (hpw)
        portYIELD_FROM_ISR();
}

/* ===================== LCD HELPERS ===================== */

static void lcd_show_two_lines(const char *l1, const char *l2)
{
    lcd_clear();
    lcd_put_cursor(0, 0);
    lcd_send_string(l1);
    lcd_put_cursor(1, 0);
    lcd_send_string(l2);
}

static void lcd_show_wrapped_text(const char *text)
{
    char l1[17] = {0};
    char l2[17] = {0};

    int len = strlen(text);
    if (len <= 16)
    {
        strncpy(l1, text, 16);
    }
    else
    {
        int split = 16;
        for (int i = 15; i >= 0; i--)
        {
            if (text[i] == ' ')
            {
                split = i;
                break;
            }
        }
        strncpy(l1, text, split);
        strncpy(l2, text + ((split < len) ? split + 1 : split), 16);
    }

    lcd_show_two_lines(l1, l2);
    vTaskDelay(pdMS_TO_TICKS(5000));
}

/* ===================== CORE 1 WORKER TASK ===================== */

static void uart_worker_task(void *arg)
{
    worker_msg_t msg;

    while (1)
    {
        if (xQueueReceive(worker_queue, &msg, portMAX_DELAY))
        {

            if (msg.type == MSG_CMD)
            {
                char l1[17], l2[17];
                snprintf(l1, sizeof(l1), "%dF", msg.tf);
                snprintf(l2, sizeof(l2), "%dC", msg.tc);

                ESP_LOGI(TAG, "Core1: LCD CMD %s %s", l1, l2);
                lcd_show_two_lines(l1, l2);

                char ack[32];
                snprintf(ack, sizeof(ack), "%dF %dC\n", msg.tf, msg.tc);
                uart_write_bytes(UART_PORT, ack, strlen(ack));
            }

            else if (msg.type == MSG_TEXT)
            {
                ESP_LOGI(TAG, "Core1: LCD TXT \"%s\"", msg.text);
                lcd_show_wrapped_text(msg.text);
                uart_write_bytes(UART_PORT, "TXT-OK\n", 7);
            }
        }
    }
}

/* ===================== CORE 0 UART RX TASK ===================== */

static void uart_rx_task(void *arg)
{
    uint8_t data[BUF_SIZE];

    while (1)
    {
        xTaskNotifyWait(0, UART_NOTIFY_BIT, NULL, portMAX_DELAY);

        int len = uart_read_bytes(UART_PORT, data, BUF_SIZE - 1,
                                  pdMS_TO_TICKS(333));
        if (len <= 0)
            continue;

        data[len] = '\0';
        ESP_LOGI(TAG, "Core0 RX: %s", data);

        /* Handshake */
        gpio_set_level(MASTER_LOOP, 1);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(MASTER_LOOP, 0);

        char *line = strtok((char *)data, "\r\n");
        while (line)
        {

            worker_msg_t msg = {0};

            if (sscanf(line, "CMD=%d %d", &msg.tf, &msg.tc) == 2)
            {
                msg.type = MSG_CMD;
                xQueueSend(worker_queue, &msg, portMAX_DELAY);
            }
            else if (strncmp(line, "TXT=", 4) == 0)
            {
                msg.type = MSG_TEXT;
                strncpy(msg.text, line + 4, sizeof(msg.text) - 1);
                xQueueSend(worker_queue, &msg, portMAX_DELAY);
            }

            line = strtok(NULL, "\r\n");
        }
    }
}

/* ===================== MAIN ===================== */

void app_main(void)
{
    /* GPIO */
    gpio_config_t in_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pin_bit_mask = (1ULL << SLAVE_IN) | (1ULL << MASTER_IN),
        .intr_type = GPIO_INTR_NEGEDGE};
    gpio_config(&in_conf);

    gpio_config_t out_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL << SLAVE_OUT) |
            (1ULL << CLOCK_OUT) |
            (1ULL << MASTER_LOOP)};
    gpio_config(&out_conf);

    /* UART */
    uart_config_t uart_config = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);

    /* LCD */
    lcd_init();
    lcd_show_two_lines("Program by", "Gregory");

    /* Queue */
    worker_queue = xQueueCreate(4, sizeof(worker_msg_t));

    /* Tasks */
    xTaskCreatePinnedToCore(
        uart_rx_task,
        "uart_rx_task",
        4096,
        NULL,
        10,
        &uart_rx_task_handle,
        0); // Core 0

    xTaskCreatePinnedToCore(
        uart_worker_task,
        "uart_worker_task",
        4096,
        NULL,
        9,
        NULL,
        1); // Core 1

    /* ISR */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MASTER_IN, master_in_isr_handler, NULL);

    ESP_LOGI(TAG, "Dual-core UART system ready");

    int count = 1;

    while (1)
    {
        gpio_set_level(SLAVE_OUT, !gpio_get_level(SLAVE_IN));

        if (count == 10)
        {
            gpio_set_level(CLOCK_OUT, 1);
            vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(CLOCK_OUT, 0);

            count = 1;
        }
        count++;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
