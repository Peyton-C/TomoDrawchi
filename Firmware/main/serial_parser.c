#include "serial_parser.h"
#include "switch_hid.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static const char *TAG = "serial_parser";

// ---------------------------------------------------------------
// Protocol:
//   h<n>\n   — hold control n (1-12, see SwitchControl)
//   r<n>\n   — release control n
//   <n>\n    — tap control n (press, then auto-release)
//
// Commands are newline-terminated so your PC can just println().
// Carriage returns are ignored for Arduino/Python sender compat.
// ---------------------------------------------------------------

#define BUF_LEN 8

static char s_buf[BUF_LEN];
static uint8_t s_len = 0;

static void process_command(const char *cmd, uint8_t len)
{
    if (len == 0) return;

    if (cmd[0] == 'h' || cmd[0] == 'H') {
        // Hold: h<n>
        uint8_t num = (uint8_t)atoi(&cmd[1]);
        if (num >= 1 && num <= CTL_COUNT) {
            ESP_LOGD(TAG, "hold %d", num);
            switch_hid_press(num);
        } else {
            ESP_LOGW(TAG, "bad button: %s", cmd);
        }

    } else if (cmd[0] == 'r' || cmd[0] == 'R') {
        // Release: r<n>
        uint8_t num = (uint8_t)atoi(&cmd[1]);
        if (num >= 1 && num <= CTL_COUNT) {
            ESP_LOGD(TAG, "release %d", num);
            switch_hid_release(num);
        } else {
            ESP_LOGW(TAG, "bad button: %s", cmd);
        }

    } else if (isdigit((unsigned char)cmd[0])) {
        // Tap: <n>
        uint8_t num = (uint8_t)atoi(cmd);
        if (num >= 1 && num <= CTL_COUNT) {
            ESP_LOGD(TAG, "tap %d", num);
            switch_hid_tap(num);
        } else {
            ESP_LOGW(TAG, "bad button: %s", cmd);
        }

    } else {
        ESP_LOGW(TAG, "unknown command: %s", cmd);
    }
}

void serial_parser_feed(char c)
{
    // Ignore carriage returns (Windows \r\n senders)
    if (c == '\r') return;

    if (c == '\n') {
        // Null-terminate and process whatever we buffered
        s_buf[s_len] = '\0';
        process_command(s_buf, s_len);
        s_len = 0;
        return;
    }

    // Buffer the character; drop if overflowing (malformed input)
    if (s_len < BUF_LEN - 1) {
        s_buf[s_len++] = c;
    } else {
        ESP_LOGW(TAG, "input overflow, discarding buffer");
        s_len = 0;
    }
}