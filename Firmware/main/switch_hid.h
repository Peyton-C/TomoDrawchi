#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

// ---------------------------------------------------------------
// Controls, in serial protocol order (protocol number = value + 1)
// Must stay in sync with ControllerBackend.py
// ---------------------------------------------------------------
typedef enum {
    CTL_A       = 0,   // "1"
    CTL_B       = 1,   // "2"
    CTL_X       = 2,   // "3"
    CTL_Y       = 3,   // "4"
    CTL_UP      = 4,   // "5"
    CTL_DOWN    = 5,   // "6"
    CTL_LEFT    = 6,   // "7"
    CTL_RIGHT   = 7,   // "8"
    CTL_PLUS    = 8,   // "9"
    CTL_MINUS   = 9,   // "10"
    CTL_R       = 10,  // "11"
    CTL_L3      = 11,  // "12"
    CTL_COUNT   = 12,
} SwitchControl;

// USB descriptors — wired HORI Pokken pad, which the Switch accepts
// without any pairing or handshake
extern const tusb_desc_device_t switch_device_desc;
extern const char *switch_string_desc[];
extern const int switch_string_desc_count;
extern const uint8_t switch_fs_config_desc[];
extern const uint8_t switch_hid_report_desc[];

// Call once after TinyUSB is up
void switch_hid_init(void);

// Press/release a control by 1-based protocol number (1–12)
// Matches the serial protocol: h1 / r1 / 1
void switch_hid_press(uint8_t ctl_num);    // hold indefinitely
void switch_hid_release(uint8_t ctl_num);  // release
void switch_hid_tap(uint8_t ctl_num);      // TAP_DURATION then release

// Called from the control task — sends queued reports and handles tap timeouts
void switch_hid_task(void);
