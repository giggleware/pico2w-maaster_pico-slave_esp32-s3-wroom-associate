#ifndef HTTP_HELPER_H
#define HTTP_HELPER_H

#include "lwip/tcp.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#define ESP32_IP "192.168.1.248"

/* ===================== SHARED STATE ===================== */

volatile uint8_t pending_cmd = 0;
volatile uint32_t slave_output = 0;
volatile uint16_t current_temp_raw = 0;
volatile uint8_t current_led_byte = 0;

#define TEXT_BUF_LEN 33 // 16x2 = 32 chars + NUL
volatile char pending_text[TEXT_BUF_LEN];
volatile uint8_t text_pending = 0;

/* ===================== HELPERS ===================== */

static err_t esp32_connected(void *arg,
                             struct tcp_pcb *pcb,
                             err_t err)
{
    const char *req = (const char *)arg;

    if (err != ERR_OK)
    {
        tcp_close(pcb);
        return err;
    }

    tcp_write(pcb, req, strlen(req), TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);

    tcp_close(pcb);
    return ERR_OK;
}

void esp32_send_http(const char *req)
{
    struct tcp_pcb *pcb = tcp_new();
    ip_addr_t ip;

    if (!pcb)
        return;

    ipaddr_aton(ESP32_IP, &ip);

    tcp_arg(pcb, (void *)req);
    tcp_connect(pcb, &ip, 80, esp32_connected);
}

static void send_empty_200(struct tcp_pcb *pcb)
{
    const char *resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    tcp_write(pcb, resp, strlen(resp), TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
}

static void send_json_status(struct tcp_pcb *pcb)
{
    char body[192];
    char header[256];

    int body_len = snprintf(body, sizeof(body),
                            "{"
                            "\"raw\":%u,"
                            "\"temperature\":%u,"
                            "\"led\":%u,"
                            "\"timestamp\":%lu"
                            "}",
                            slave_output,
                            current_temp_raw,
                            current_led_byte,
                            (unsigned long)time(NULL));

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n\r\n",
                              body_len);

    tcp_write(pcb, header, header_len, TCP_WRITE_FLAG_COPY);
    tcp_write(pcb, body, body_len, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
}

/* ===================== HTTP HANDLER ===================== */

static err_t http_handler(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    (void)arg;
    (void)err;

    if (!p)
    {
        tcp_close(pcb);
        return ERR_OK;
    }

    char req[512];
    int len = (p->tot_len < sizeof(req) - 1) ? p->tot_len : sizeof(req) - 1;
    pbuf_copy_partial(p, req, len, 0);
    req[len] = '\0';

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    /* ---------- GET /api/status ---------- */
    if (strncmp(req, "GET /api/status", 15) == 0)
    {
        send_json_status(pcb);
        tcp_close(pcb);
        return ERR_OK;
    }

    /* ---------- POST /api/control ---------- */
    if (strncmp(req, "POST /api/control", 17) == 0)
    {
        char *body = strstr(req, "\r\n\r\n");
        if (body)
        {
            body += 4;
            cJSON *json = cJSON_Parse(body);
            if (json)
            {
                cJSON *led = cJSON_GetObjectItem(json, "led");
                if (cJSON_IsNumber(led))
                {
                    pending_cmd = led->valueint & 0xFF;
                }
                cJSON_Delete(json);
            }
        }

        send_empty_200(pcb);
        tcp_close(pcb);
        return ERR_OK;
    }

    /* ---------- POST /api/text ---------- */
    if (strstr(req, "POST /api/text") != NULL)
    {

        /* 1. Extract body from incoming request */
        char *body = strstr(req, "\r\n\r\n");
        if (!body)
        {
            printf("Body empty\n");
            send_empty_200(pcb);
            tcp_close(pcb);
            return ERR_OK;
        }
        body += 4;

        /* 2. Parse JSON */
        cJSON *json = cJSON_Parse(body);
        if (!json)
        {
            printf("Bad JSON\n");
            send_empty_200(pcb);
            tcp_close(pcb);
            return ERR_OK;
        }

        cJSON *txt = cJSON_GetObjectItem(json, "text");
        if (!cJSON_IsString(txt))
        {
            printf("Missing text field\n");
            cJSON_Delete(json);
            send_empty_200(pcb);
            tcp_close(pcb);
            return ERR_OK;
        }

        /* 3. Build POST body for ESP32 */
        char esp_body[128];
        snprintf(esp_body, sizeof(esp_body),
                 "{\"text\":\"%s\"}", txt->valuestring);

        cJSON_Delete(json);

        /* 4. Build HTTP request to ESP32 */
        char esp_req[256];
        snprintf(esp_req, sizeof(esp_req),
                 "POST /api/text HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s",
                 ESP32_IP,
                 strlen(esp_body),
                 esp_body);

        /* 5. Send to ESP32 */
        esp32_send_http(esp_req); // ← your existing TCP send function

        /* 6. Reply to original client */
        send_empty_200(pcb);
        tcp_close(pcb);
        return ERR_OK;
    }
}

/* ===================== ACCEPT CALLBACK ===================== */

static err_t accept_callback(void *arg, struct tcp_pcb *client, err_t err)
{
    (void)arg;
    (void)err;
    tcp_recv(client, http_handler);
    return ERR_OK;
}

#endif
