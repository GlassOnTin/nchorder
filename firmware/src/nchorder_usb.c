/**
 * Northern Chorder - USB HID Driver
 *
 * Implements USB keyboard HID using Nordic SDK app_usbd
 */

#include "nchorder_usb.h"
#include "nchorder_config.h"
#include "nchorder_msc.h"

#include "app_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd_hid_kbd.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "SEGGER_RTT.h"

// USB HID keyboard interface number
#define NCHORDER_USB_INTERFACE_KBD   0

// USB connection state
static bool m_usb_connected = false;
static bool m_usb_suspended = false;

// Forward declaration of event handler
static void hid_kbd_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                    app_usbd_hid_user_event_t event);

/**
 * USB HID Keyboard instance
 * Uses endpoint 1 for HID reports
 */
APP_USBD_HID_KBD_GLOBAL_DEF(m_app_hid_kbd,
                            NCHORDER_USB_INTERFACE_KBD,
                            NRF_DRV_USBD_EPIN1,
                            hid_kbd_user_ev_handler,
                            APP_USBD_HID_SUBCLASS_BOOT);

/**
 * HID keyboard class event handler
 */
static void hid_kbd_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                    app_usbd_hid_user_event_t event)
{
    UNUSED_PARAMETER(p_inst);

    switch (event)
    {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
            // LED status report from host (Caps Lock, Num Lock, etc.)
            // Could update LEDs here if needed
            NRF_LOG_DEBUG("USB: LED report received");
            break;

        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
            NRF_LOG_DEBUG("USB: HID report sent");
            break;

        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
            NRF_LOG_INFO("USB: Boot protocol set");
            UNUSED_RETURN_VALUE(hid_kbd_clear_buffer(p_inst));
            break;

        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
            NRF_LOG_INFO("USB: Report protocol set");
            UNUSED_RETURN_VALUE(hid_kbd_clear_buffer(p_inst));
            break;

        default:
            break;
    }
}

/**
 * USB device event handler
 */
static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    switch (event)
    {
        case APP_USBD_EVT_DRV_SOF:
            // Start of frame - happens every 1ms when connected
            break;

        case APP_USBD_EVT_DRV_SUSPEND:
            NRF_LOG_INFO("USB: Suspended");
            m_usb_suspended = true;
            app_usbd_suspend_req();
#if defined(BOARD_TWIDDLER4) || defined(BOARD_XIAO_NRF52840)
            // Request config reload when USB is suspended (host ejected the drive)
            nchorder_msc_on_disconnect();
#endif
            break;

        case APP_USBD_EVT_DRV_RESUME:
            NRF_LOG_INFO("USB: Resumed");
            m_usb_suspended = false;
#if defined(BOARD_TWIDDLER4) || defined(BOARD_XIAO_NRF52840)
            // Mark USB as active (host resumed communication)
            nchorder_msc_set_active();
#endif
            break;

        case APP_USBD_EVT_STARTED:
            NRF_LOG_INFO("USB: Started");
            m_usb_connected = true;
            break;

        case APP_USBD_EVT_STOPPED:
            NRF_LOG_INFO("USB: Stopped");
            m_usb_connected = false;
            app_usbd_disable();
#if defined(BOARD_TWIDDLER4) || defined(BOARD_XIAO_NRF52840)
            // Request config reload when USB stops
            nchorder_msc_on_disconnect();
#endif
            break;

        case APP_USBD_EVT_POWER_DETECTED:
            NRF_LOG_INFO("USB: Power detected");
            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;

        case APP_USBD_EVT_POWER_REMOVED:
            NRF_LOG_INFO("USB: Power removed");
            m_usb_connected = false;
            app_usbd_stop();
#if defined(BOARD_TWIDDLER4) || defined(BOARD_XIAO_NRF52840)
            // Request config reload when USB power is removed
            nchorder_msc_on_disconnect();
#endif
            break;

        case APP_USBD_EVT_POWER_READY:
            NRF_LOG_INFO("USB: Power ready, starting");
            app_usbd_start();
            break;

        default:
            break;
    }
}

uint32_t nchorder_usb_init(void)
{
    ret_code_t ret;

    SEGGER_RTT_printf(0, "USB: Start init\n");

    // Initialize clock driver (required by USB)
    ret = nrf_drv_clock_init();
    SEGGER_RTT_printf(0, "USB: clock_init ret=%d\n", ret);
    if ((ret != NRF_SUCCESS) && (ret != NRF_ERROR_MODULE_ALREADY_INITIALIZED))
    {
        return ret;
    }

    // Request HFCLK (required for USB)
    SEGGER_RTT_printf(0, "USB: HFCLK running=%d\n", nrf_drv_clock_hfclk_is_running());
    if (!nrf_drv_clock_hfclk_is_running())
    {
        nrf_drv_clock_hfclk_request(NULL);
        while (!nrf_drv_clock_hfclk_is_running())
        {
            // Wait for HFCLK to start
        }
        SEGGER_RTT_printf(0, "USB: HFCLK started\n");
    }

    // USB device configuration
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler,
    };

    SEGGER_RTT_printf(0, "USB: Calling app_usbd_init\n");
    // Initialize USB device library
    ret = app_usbd_init(&usbd_config);
    SEGGER_RTT_printf(0, "USB: app_usbd_init ret=%d\n", ret);
    if (ret != NRF_SUCCESS)
    {
        return ret;
    }

    // Get keyboard class instance and register it
    app_usbd_class_inst_t const * class_inst_kbd;
    class_inst_kbd = app_usbd_hid_kbd_class_inst_get(&m_app_hid_kbd);

    ret = app_usbd_class_append(class_inst_kbd);
    if (ret != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("USB: class_append failed: %d", ret);
        return ret;
    }

    NRF_LOG_INFO("USB: Init complete (call nchorder_usb_start after adding all classes)");
    return NRF_SUCCESS;
}

uint32_t nchorder_usb_start(void)
{
#if defined(BOARD_XIAO_NRF52840)
    // XIAO: Skip power events (crashes with SoftDevice), manually start USB
    // USB is always connected when XIAO is plugged in
    NRF_LOG_INFO("USB: Manual start (XIAO, no power detection)");
    app_usbd_enable();
    app_usbd_start();

    // Process events to actually start USB (enable D+ pullup)
    // The start request is queued and processed asynchronously
    for (int i = 0; i < 100; i++)
    {
        while (app_usbd_event_queue_process())
        {
            // Process all pending events
        }
        if (nrf_drv_usbd_is_started())
        {
            SEGGER_RTT_printf(0, "USB: Pullup enabled after %d iterations\n", i);
            break;
        }
        nrf_delay_ms(1);
    }

    m_usb_connected = nrf_drv_usbd_is_started();
    if (!m_usb_connected)
    {
        NRF_LOG_WARNING("USB: Failed to start (pullup not enabled)");
        return NRF_ERROR_INTERNAL;
    }
#else
    // Other boards: Use power detection to start USB when cable connected
    ret_code_t ret = app_usbd_power_events_enable();
    if (ret != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("USB: power_events_enable failed: %d", ret);
        return ret;
    }
#endif

    NRF_LOG_INFO("USB: Started");
    return NRF_SUCCESS;
}

bool nchorder_usb_is_connected(void)
{
    return m_usb_connected && !m_usb_suspended;
}

uint32_t nchorder_usb_key_press(uint8_t modifiers, uint8_t keycode)
{
    ret_code_t ret;

    if (!nchorder_usb_is_connected())
    {
        return NRF_ERROR_INVALID_STATE;
    }

    // Clear any previous state
    UNUSED_RETURN_VALUE(app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_CTRL, false));
    UNUSED_RETURN_VALUE(app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_SHIFT, false));
    UNUSED_RETURN_VALUE(app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_ALT, false));
    UNUSED_RETURN_VALUE(app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_UI, false));

    // Set modifiers
    if (modifiers & 0x01)  // Left Ctrl
    {
        ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
            APP_USBD_HID_KBD_MODIFIER_LEFT_CTRL, true);
        if (ret != NRF_SUCCESS) return ret;
    }
    if (modifiers & 0x02)  // Left Shift
    {
        ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
            APP_USBD_HID_KBD_MODIFIER_LEFT_SHIFT, true);
        if (ret != NRF_SUCCESS) return ret;
    }
    if (modifiers & 0x04)  // Left Alt
    {
        ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
            APP_USBD_HID_KBD_MODIFIER_LEFT_ALT, true);
        if (ret != NRF_SUCCESS) return ret;
    }
    if (modifiers & 0x08)  // Left GUI
    {
        ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
            APP_USBD_HID_KBD_MODIFIER_LEFT_UI, true);
        if (ret != NRF_SUCCESS) return ret;
    }

    // Press key
    if (keycode != 0)
    {
        ret = app_usbd_hid_kbd_key_control(&m_app_hid_kbd, keycode, true);
        if (ret != NRF_SUCCESS)
        {
            NRF_LOG_WARNING("USB: key_control press failed: %d", ret);
            return ret;
        }
    }

    return NRF_SUCCESS;
}

uint32_t nchorder_usb_key_release(void)
{
    ret_code_t ret;

    if (!nchorder_usb_is_connected())
    {
        return NRF_ERROR_INVALID_STATE;
    }

    // Release all keys (clear the keyboard state)
    // The SDK tracks pressed keys, so we need to release them all
    for (uint8_t key = 0x04; key < 0x68; key++)
    {
        UNUSED_RETURN_VALUE(app_usbd_hid_kbd_key_control(&m_app_hid_kbd, key, false));
    }

    // Clear all modifiers
    ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_CTRL, false);
    ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_SHIFT, false);
    ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_ALT, false);
    ret = app_usbd_hid_kbd_modifier_state_set(&m_app_hid_kbd,
        APP_USBD_HID_KBD_MODIFIER_LEFT_UI, false);

    return ret;
}

void nchorder_usb_process(void)
{
    // Process USB events when using app_usbd with scheduler
    while (app_usbd_event_queue_process())
    {
        // Process all pending events
    }
}
