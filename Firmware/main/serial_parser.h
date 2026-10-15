#pragma once
#include <stdint.h>

// Feed one character at a time from the UART ISR/task.
// Internally buffers until a full command is recognised,
// then calls switch_hid_press / release / tap directly.
void serial_parser_feed(char c);