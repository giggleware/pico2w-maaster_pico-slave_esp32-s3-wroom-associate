#ifndef CORE_HELPER_H
#define CORE_HELPER_H

// This is core_helper.h, a header file I made to handle the Core1 work.
// I put all of the cyw3 wi-fi code into Core1. The Pico 2W wi-fi management now has a whole
// core to itself. That frees up Core0 (the main cor) for routine communication with the slave.
// Core0 and Core1 also communicate with each other. Core0, the main cor, generates output values
// like ADC values (thermistor from slave), and the state of the LEDs (red = 4, yellow = 2, green = 1,
// or any three bit combination stuffed into an eight bit byte. I call this the accessory control byte.)

// ChatGPT code:
void core1_main(void)
{
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();

    cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        30000);

    // start TCP server
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("tcp_new failed\n");
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("tcp_bind failed\n");
    }
    pcb = tcp_listen(pcb);
    if (!pcb)
    {
        printf("tcp_listen failed\n");
    }
    tcp_accept(pcb, accept_callback);
    printf("Web server running at http://%s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    sleep_ms(200);
    printf("IP: %s\n", netif_default ? ip4addr_ntoa(netif_ip4_addr(netif_default)) : "0.0.0.0");

    sleep_ms(100);
    printf("MASTER READY\n");

    while (true)
    {
        cyw43_arch_poll(); // 🔑 THIS IS REQUIRED
        sleep_ms(1);
    }
}

#endif