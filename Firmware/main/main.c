#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

#include "switch_hid.h"
#include "serial_parser.h"

static const char *TAG = "main";

// ---------------------------------------------------------------
// Command UART — an external 3.3V USB-UART adapter, wired with
// GND + data only (never its 5V/3V3 pin: the board is powered by
// the Switch, and a second 5V source stops USB-C hosts attaching)
//   adapter TX  -> GPIO1 (our RX)
//   adapter GND -> GND
//   adapter RX  -> GPIO2 (our TX, optional — nothing reads it yet)
// UART0 (the on-board "UART" port) stays free for flashing + logs
// ---------------------------------------------------------------
#define SERIAL_UART_NUM     UART_NUM_1
#define SERIAL_UART_RX_PIN  1
#define SERIAL_UART_TX_PIN  2
#define SERIAL_BAUD         115200
#define SERIAL_BUF_SIZE     256

// ---------------------------------------------------------------
// Control task
// Drains the UART ring buffer into the parser, then handles tap
// timeouts and report sends. One task for both, so the HID state
// is only ever touched from here.
// ---------------------------------------------------------------
static void control_task(void *arg)
{
    uint8_t buf[64];
    while (1) {
        // 1 tick = 1ms (CONFIG_FREERTOS_HZ=1000), which paces the loop
        int n = uart_read_bytes(SERIAL_UART_NUM, buf, sizeof(buf), 1);
        for (int i = 0; i < n; i++) {
            serial_parser_feed((char)buf[i]);
        }
        switch_hid_task();
    }
}

// ---------------------------------------------------------------
// USB bus events — logged so the UART monitor shows what the
// Switch is doing with us
// ---------------------------------------------------------------
static void usb_event_cb(tinyusb_event_t *event, void *arg)
{
    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:  ESP_LOGI(TAG, "USB: configured by host"); break;
    case TINYUSB_EVENT_DETACHED:  ESP_LOGI(TAG, "USB: detached");           break;
    case TINYUSB_EVENT_SUSPENDED: ESP_LOGI(TAG, "USB: suspended");          break;
    case TINYUSB_EVENT_RESUMED:   ESP_LOGI(TAG, "USB: resumed");            break;
    default:                      ESP_LOGI(TAG, "USB: event %d", event->id); break;
    }
}

// ---------------------------------------------------------------
// App entry point
// ---------------------------------------------------------------
void app_main(void)
{
    // --- UART init ---
    const uart_config_t uart_cfg = {
        .baud_rate  = SERIAL_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(SERIAL_UART_NUM,
                                        SERIAL_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(SERIAL_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(SERIAL_UART_NUM,
                                 SERIAL_UART_TX_PIN, SERIAL_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // --- TinyUSB init (the driver runs the stack in its own task) ---
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device            = &switch_device_desc;
    tusb_cfg.descriptor.string            = switch_string_desc;
    tusb_cfg.descriptor.string_count      = switch_string_desc_count;
    tusb_cfg.descriptor.full_speed_config = switch_fs_config_desc;
    tusb_cfg.event_cb                     = usb_event_cb;
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "TinyUSB started — waiting for Switch enumeration");

    // --- HID state init ---
    switch_hid_init();

    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready. Send h<n>, r<n>, or <n> over serial.");
}
