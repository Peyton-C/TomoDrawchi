#include "switch_hid.h"
#include "class/hid/hid_device.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include "tusb.h"

static const char *TAG = "switch_hid";

// ---------------------------------------------------------------
// Device + string descriptors — HORI Pokken Tournament pad
// ---------------------------------------------------------------
const tusb_desc_device_t switch_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x0f0d,
    .idProduct          = 0x0092,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01,
};

const char *switch_string_desc[] = {
    (const char[]){0x09, 0x04},  // 0: language (English)
    "HORI CO.,LTD.",             // 1: manufacturer
    "POKKEN CONTROLLER",         // 2: product
};
const int switch_string_desc_count =
    sizeof(switch_string_desc) / sizeof(switch_string_desc[0]);

// ---------------------------------------------------------------
// HID Report Descriptor — HORI Pokken pad (Switch-compatible)
// ---------------------------------------------------------------
const uint8_t switch_hid_report_desc[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Gamepad)
    0xa1, 0x01,        // Collection (Application)

    // 16 buttons
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x01,        //   Physical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x10,        //   Usage Maximum (Button 16)
    0x81, 0x02,        //   Input (Data, Var, Abs)

    // Hat switch (4-bit, 0=N … 7=NW, 8=neutral)
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x25, 0x07,        //   Logical Maximum (7)
    0x46, 0x3b, 0x01,  //   Physical Maximum (315)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x65, 0x14,        //   Unit (Eng Rot)
    0x09, 0x39,        //   Usage (Hat Switch)
    0x81, 0x42,        //   Input (Data, Var, Abs, Null)

    // 4-bit padding (fills out the hat byte)
    0x65, 0x00,        //   Unit (None)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const)

    // 4 axes: LX, LY, RX, RY  (0x00–0xFF, centre = 0x80)
    0x26, 0xff, 0x00,  //   Logical Maximum (255)
    0x46, 0xff, 0x00,  //   Physical Maximum (255)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data, Var, Abs)

    // 1 vendor byte (Switch expects this)
    0x06, 0x00, 0xff,  //   Usage Page (Vendor 0xFF00)
    0x09, 0x20,        //   Usage (0x20)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data, Var, Abs)

    // 8-byte vendor output report (never used, but part of the real pad)
    0x0a, 0x21, 0x26,  //   Usage (0x2621)
    0x95, 0x08,        //   Report Count (8)
    0x91, 0x02,        //   Output (Data, Var, Abs)

    0xc0               // End Collection
};

#define SWITCH_HID_CONFIG_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

const uint8_t switch_fs_config_desc[] = {
    // bConfigurationValue, bNumInterfaces, iConfiguration, total length,
    // bmAttributes, bMaxPower (mA)
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, SWITCH_HID_CONFIG_DESC_LEN, 0x00, 100),

    // bInterfaceNumber, iInterface, bInterfaceProtocol,
    // wDescriptorLength, EP address, EP size, bInterval (ms)
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(switch_hid_report_desc),
                       0x81,                   // EP1 IN
                       CFG_TUD_HID_EP_BUFSIZE,
                       1)                      // 1ms polling = lowest latency
};

// ---------------------------------------------------------------
// Report struct — must match descriptor byte-for-byte
// Total: 2 (buttons) + 1 (hat+pad) + 4 (axes) + 1 (vendor) = 8 bytes
// ---------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    uint8_t  hat;      // 0-7 = direction, 8 = neutral
    uint8_t  lx;
    uint8_t  ly;
    uint8_t  rx;
    uint8_t  ry;
    uint8_t  vendor;   // always 0x00
} switch_report_t;

// Button bits in the report, as the Switch reads them
#define BTN_Y       (1u << 0)
#define BTN_B       (1u << 1)
#define BTN_A       (1u << 2)
#define BTN_X       (1u << 3)
#define BTN_L       (1u << 4)
#define BTN_R       (1u << 5)
#define BTN_ZL      (1u << 6)
#define BTN_ZR      (1u << 7)
#define BTN_MINUS   (1u << 8)
#define BTN_PLUS    (1u << 9)
#define BTN_LS      (1u << 10)  // Left stick click
#define BTN_RS      (1u << 11)  // Right stick click
#define BTN_HOME    (1u << 12)
#define BTN_CAPTURE (1u << 13)

#define HAT_NEUTRAL 8

// Report button bit for each control; 0 = D-Pad, which goes in the hat
static const uint16_t s_ctl_button[CTL_COUNT] = {
    [CTL_A]     = BTN_A,
    [CTL_B]     = BTN_B,
    [CTL_X]     = BTN_X,
    [CTL_Y]     = BTN_Y,
    [CTL_PLUS]  = BTN_PLUS,
    [CTL_MINUS] = BTN_MINUS,
    [CTL_R]     = BTN_R,
    [CTL_L3]    = BTN_LS,
};

// Matches PULSE_MS from the old Pico firmware
#define TAP_DURATION_US  (30 * 1000)

// Bit per control: currently pressed (held or mid-tap)
static uint16_t s_pressed = 0;

// Per-control tap release time; 0 = no pending tap
static int64_t s_tap_release_us[CTL_COUNT] = {0};

static bool s_dirty = false;

// ---------------------------------------------------------------

void switch_hid_init(void)
{
    s_pressed = 0;
    memset(s_tap_release_us, 0, sizeof(s_tap_release_us));
    s_dirty = true;  // send one neutral report once the Switch is ready
}

// ctl_num is 1-based from the serial protocol
static inline int ctl_index(uint8_t ctl_num)
{
    return (int)ctl_num - 1;  // convert to 0-based
}

void switch_hid_press(uint8_t ctl_num)
{
    int i = ctl_index(ctl_num);
    if (i < 0 || i >= CTL_COUNT) return;

    s_tap_release_us[i] = 0;  // cancel any pending tap on this control
    s_pressed |= (1u << i);
    s_dirty = true;
}

void switch_hid_release(uint8_t ctl_num)
{
    int i = ctl_index(ctl_num);
    if (i < 0 || i >= CTL_COUNT) return;

    s_tap_release_us[i] = 0;
    s_pressed &= ~(1u << i);
    s_dirty = true;
}

void switch_hid_tap(uint8_t ctl_num)
{
    int i = ctl_index(ctl_num);
    if (i < 0 || i >= CTL_COUNT) return;

    s_pressed |= (1u << i);
    s_tap_release_us[i] = esp_timer_get_time() + TAP_DURATION_US;
    s_dirty = true;
}

static uint8_t hat_from_dpad(void)
{
    // Opposite directions cancel out
    int dx = !!(s_pressed & (1u << CTL_RIGHT)) - !!(s_pressed & (1u << CTL_LEFT));
    int dy = !!(s_pressed & (1u << CTL_DOWN))  - !!(s_pressed & (1u << CTL_UP));

    // Indexed [dy + 1][dx + 1]
    static const uint8_t hat[3][3] = {
        {7, 0, 1},            // up-left,   up,      up-right
        {6, HAT_NEUTRAL, 2},  // left,      neutral, right
        {5, 4, 3},            // down-left, down,    down-right
    };
    return hat[dy + 1][dx + 1];
}

static void build_report(switch_report_t *report)
{
    report->buttons = 0;
    for (int i = 0; i < CTL_COUNT; i++) {
        if (s_pressed & (1u << i)) {
            report->buttons |= s_ctl_button[i];
        }
    }
    report->hat    = hat_from_dpad();
    report->lx     = 0x80;
    report->ly     = 0x80;
    report->rx     = 0x80;
    report->ry     = 0x80;
    report->vendor = 0x00;
}

void switch_hid_task(void)
{
    // Check for expired tap timers and release those controls
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < CTL_COUNT; i++) {
        if (s_tap_release_us[i] != 0 && now >= s_tap_release_us[i]) {
            s_pressed &= ~(1u << i);
            s_tap_release_us[i] = 0;
            s_dirty = true;
        }
    }

    // Push report to Switch if something changed and USB is ready
    if (s_dirty && tud_hid_ready()) {
        switch_report_t report;
        build_report(&report);
        tud_hid_report(0, &report, sizeof(report));
        s_dirty = false;
    } else if (s_dirty) {
        static int64_t last_warn_us = 0;
        if (now - last_warn_us > 1000000) {
            ESP_LOGW(TAG, "report pending: mounted=%d suspended=%d",
                     tud_mounted(), tud_suspended());
            last_warn_us = now;
        }
    }
}

// ---------------------------------------------------------------
// TinyUSB HID callbacks (required by the stack)
// ---------------------------------------------------------------

// The stack calls this to get our report descriptor on enumeration
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return switch_hid_report_desc;
}

// Called when the host requests a report (e.g. on connection)
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    ESP_LOGD(TAG, "GET_REPORT id=%d type=%d len=%d", report_id, report_type, reqlen);
    switch_report_t report;
    build_report(&report);
    if (reqlen < sizeof(report)) return 0;
    memcpy(buffer, &report, sizeof(report));
    return sizeof(report);
}

// We don't accept output/feature reports from the Switch
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance; (void)buffer;
    ESP_LOGD(TAG, "SET_REPORT id=%d type=%d len=%d", report_id, report_type, bufsize);
}
