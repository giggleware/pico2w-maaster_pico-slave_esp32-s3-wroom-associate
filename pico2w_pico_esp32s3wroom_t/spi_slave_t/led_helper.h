#ifndef LED_HELPER_H
#define LED_HELPER_H

const uint LED_R = 0;
const uint LED_Y = 1;
const uint LED_G = 2;
const uint LED_B = 3;
const uint LED_EXT = 4;

int initLEDs()
{
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(LED_Y);
    gpio_set_dir(LED_Y, GPIO_OUT);
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_init(LED_EXT);
    gpio_set_dir(LED_EXT, GPIO_OUT);
}

int zeroLEDs()
{
    gpio_put(LED_R, 0);
    gpio_put(LED_Y, 0);
    gpio_put(LED_G, 0);
    gpio_put(LED_B, 0);
    gpio_put(LED_EXT, 0);
}

static bool get_led_state(uint led_target)
{
    bool state = gpio_get(led_target);
    return state;
}

static void led_on(uint led_target)
{
    gpio_put(led_target, 1);
}

static void led_off(uint led_target)
{
    gpio_put(led_target, 0);
}

void toggleLED(uint8_t led_id, uint8_t color_number)
{
    bool state = get_led_state(led_id);
    char ledLabel[10];
    printf("LED id: %d\n", led_id);
    switch (color_number)
    {
    case 1:
        strcpy(ledLabel, "GREEN");
        break;
    case 2:
        strcpy(ledLabel, "YELLOW");
        break;
    case 3:
        strcpy(ledLabel, "RED");
        break;
    case 4:
        strcpy(ledLabel, "LED_B");
        break;
    case 5:
        strcpy(ledLabel, "LED_EXT");
        break;
    default: // do nothing
    }

    if (state == 1)
    {
        led_off(led_id);
        printf("LED TOGGLED OFF: %d %s\n", led_id, ledLabel);
    }
    else
    {
        led_on(led_id);
        printf("LED TOGGLED ON: %d %s\n", led_id, ledLabel);
    }
}
#endif