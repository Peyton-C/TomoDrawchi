#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#define PIN_COUNT 12
#define PULSE_MS  30

const uint pins[PIN_COUNT] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

uint32_t pin_timers[PIN_COUNT] = {0};
bool pin_active[PIN_COUNT] = {false};
bool pin_held[PIN_COUNT] = {false};

void release_pin(int i) {
    gpio_put(pins[i], 1);
    pin_active[i] = false;
    pin_held[i] = false;
    printf("Pin %d returned HIGH.\n", pins[i]);
}

int main() {
    stdio_usb_init();

    for (int i = 0; i < PIN_COUNT; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 1);
    }

    char buf[16];
    int buf_pos = 0;

    printf("Ready.\n");
    printf("  1-13:  pulse pin LOW\n");
    printf("  h1-13: hold pin LOW\n");
    printf("  r1-13: release held pin\n");

    while (true) {
        // Check timers, skip held pins
        uint32_t now = to_ms_since_boot(get_absolute_time());
        for (int i = 0; i < PIN_COUNT; i++) {
            if (pin_active[i] && !pin_held[i] && (now - pin_timers[i] >= PULSE_MS)) {
                release_pin(i);
            }
        }

        // Non-blocking serial read
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\n' || c == '\r') {
                if (buf_pos > 0) {
                    buf[buf_pos] = '\0';
                    buf_pos = 0;

                    char mode = 'p'; // p = pulse, h = hold, r = release
                    char *num_str = buf;

                    if (buf[0] == 'h' || buf[0] == 'r') {
                        mode = buf[0];
                        num_str = buf + 1;
                    }

                    int val = atoi(num_str);

                    if (val >= 1 && val <= PIN_COUNT) {
                        int i = val - 1;

                        if (mode == 'r') {
                            if (pin_held[i]) {
                                release_pin(i);
                            } else {
                                printf("Pin %d is not held.\n", pins[i]);
                            }
                        } else if (mode == 'h') {
                            gpio_put(pins[i], 0);
                            pin_active[i] = true;
                            pin_held[i] = true;
                            printf("Pin %d held LOW.\n", pins[i]);
                        } else {
                            // Pulse — don't override a held pin
                            if (pin_held[i]) {
                                printf("Pin %d is held. Release it with r%d first.\n", pins[i], val);
                            } else {
                                gpio_put(pins[i], 0);
                                pin_timers[i] = to_ms_since_boot(get_absolute_time());
                                pin_active[i] = true;
                                printf("Pin %d pulled LOW for %dms.\n", pins[i], PULSE_MS);
                            }
                        }
                    } else {
                        printf("Invalid input.\n");
                    }
                }
            } else if (buf_pos < (int)(sizeof(buf) - 1)) {
                buf[buf_pos++] = (char)c;
            }
        }
    }
}