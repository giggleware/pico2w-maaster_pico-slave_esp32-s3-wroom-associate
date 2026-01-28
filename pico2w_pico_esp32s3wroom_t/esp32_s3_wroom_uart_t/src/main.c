#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "i2c_lcd.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* ===================== CONFIG ===================== */

#define WIFI_SSID "WIFI_NAME"
#define WIFI_PASS "WIFI_PASSWORD"

#define SLAVE_IN 4
#define SLAVE_OUT 5
#define CLOCK_OUT 6
#define MASTER_IN 7
#define MASTER_LOOP 9

static const char *TAG = "ESP32_HTTP";

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

    size_t len = strlen(text);

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
}

/* ===================== HTTP BODY RECV ===================== */

static esp_err_t recv_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    int remaining = req->content_len;
    int received = 0;

    while (remaining > 0)
    {
        int r = httpd_req_recv(req,
                               buf + received,
                               MIN(remaining, buf_len - received - 1));
        if (r <= 0)
            return ESP_FAIL;

        received += r;
        remaining -= r;
    }

    buf[received] = '\0';
    return ESP_OK;
}

/* ===================== HTTP HANDLERS ===================== */

static esp_err_t text_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, ">>> POST /api/text <<<");

    char buf[256];

    /* Read entire HTTP body ONCE */
    if (recv_body(req, buf, sizeof(buf)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to receive body");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body error");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "RAW BODY: %s", buf);

    cJSON *json = cJSON_Parse(buf);
    if (!json)
    {
        ESP_LOGE(TAG, "JSON parse failed");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_OK;
    }

    cJSON *txt = cJSON_GetObjectItem(json, "text");
    if (!cJSON_IsString(txt))
    {
        ESP_LOGE(TAG, "Missing 'text' field");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing text");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "TEXT RECEIVED: \"%s\"", txt->valuestring);
    lcd_show_wrapped_text(txt->valuestring);

    cJSON_Delete(json);
    httpd_resp_sendstr(req, "{\"ok\":true}");

    gpio_set_level(MASTER_LOOP, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(MASTER_LOOP, 0);

    return ESP_OK;
}

static esp_err_t cmd_post_handler(httpd_req_t *req)
{
    char buf[128];

    if (recv_body(req, buf, sizeof(buf)) != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body error");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "POST /api/cmd BODY: %s", buf);

    cJSON *json = cJSON_Parse(buf);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_OK;
    }

    cJSON *tf = cJSON_GetObjectItem(json, "tf");
    cJSON *tc = cJSON_GetObjectItem(json, "tc");

    if (!cJSON_IsNumber(tf) || !cJSON_IsNumber(tc))
    {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
        return ESP_OK;
    }

    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "%dF", tf->valueint);
    snprintf(l2, sizeof(l2), "%dC", tc->valueint);

    lcd_show_two_lines(l1, l2);

    cJSON_Delete(json);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char resp[256];

    snprintf(resp, sizeof(resp),
             "{"
             "\"status\":\"ok\","
             "\"uptime_ms\":%llu,"
             "\"free_heap\":%lu"
             "}",
             esp_timer_get_time() / 1000,
             esp_get_free_heap_size());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ===================== HTTP SERVER ===================== */

static void start_http_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t text_uri = {
        .uri = "/api/text",
        .method = HTTP_POST,
        .handler = text_post_handler};

    httpd_uri_t cmd_uri = {
        .uri = "/api/cmd",
        .method = HTTP_POST,
        .handler = cmd_post_handler};

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler};

    httpd_register_uri_handler(server, &text_uri);
    httpd_register_uri_handler(server, &cmd_uri);
    httpd_register_uri_handler(server, &status_uri);

    ESP_LOGI(TAG, "HTTP server started");
}

/* ===================== WIFI ===================== */

static void wifi_event_handler(void *arg,
                               esp_event_base_t base,
                               int32_t id,
                               void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi started");
        lcd_show_two_lines("WiFi", "Connecting...");
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "WiFi disconnected");
        lcd_show_two_lines("WiFi lost", "Retrying...");
        esp_wifi_connect();
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        char ip[16];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));

        ESP_LOGI(TAG, "Got IP: %s", ip);
        lcd_show_two_lines("WiFi OK", ip);
        start_http_server();
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        }};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
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

    ESP_ERROR_CHECK(nvs_flash_init());

    lcd_init();
    lcd_show_two_lines("ESP32 Ready", "Waiting...");

    wifi_init_sta();
}
