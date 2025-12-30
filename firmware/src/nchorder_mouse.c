/**
 * Northern Chorder - USB HID Mouse Driver
 *
 * Implements mouse control via the square Trill sensor.
 * Runs always-on in parallel with chord typing.
 */

#include "nchorder_mouse.h"
#include "nchorder_config.h"

#include "app_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd_hid_mouse.h"
#include "nrf_log.h"

// Mouse interface and endpoint (after keyboard=0/EP1, CDC=1-2/EP2-3)
#define NCHORDER_USB_INTERFACE_MOUSE   3
#define NCHORDER_MOUSE_BUTTON_COUNT    3  // Left, right, middle

// Forward declaration
static void hid_mouse_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                      app_usbd_hid_user_event_t event);

/**
 * HID Mouse instance
 * Uses interface 3, endpoint 4
 */
APP_USBD_HID_MOUSE_GLOBAL_DEF(m_app_hid_mouse,
                              NCHORDER_USB_INTERFACE_MOUSE,
                              NRF_DRV_USBD_EPIN4,
                              NCHORDER_MOUSE_BUTTON_COUNT,
                              hid_mouse_user_ev_handler,
                              APP_USBD_HID_SUBCLASS_BOOT);

// State
static bool m_mouse_ready = false;

/**
 * Mouse HID event handler
 */
static void hid_mouse_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                      app_usbd_hid_user_event_t event)
{
    UNUSED_PARAMETER(p_inst);

    switch (event)
    {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
            // No output reports for mouse
            break;

        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
            NRF_LOG_DEBUG("Mouse: report sent");
            break;

        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
            NRF_LOG_INFO("Mouse: Boot protocol set");
            UNUSED_RETURN_VALUE(hid_mouse_clear_buffer(p_inst));
            break;

        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
            NRF_LOG_INFO("Mouse: Report protocol set");
            UNUSED_RETURN_VALUE(hid_mouse_clear_buffer(p_inst));
            break;

        default:
            break;
    }
}

uint32_t nchorder_mouse_init(void)
{
    ret_code_t ret;

    app_usbd_class_inst_t const * class_inst_mouse;
    class_inst_mouse = app_usbd_hid_mouse_class_inst_get(&m_app_hid_mouse);

    ret = app_usbd_class_append(class_inst_mouse);
    if (ret != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Mouse: class_append failed: %d", ret);
        return ret;
    }

    m_mouse_ready = true;
    NRF_LOG_INFO("Mouse: Initialized on interface %d", NCHORDER_USB_INTERFACE_MOUSE);
    return NRF_SUCCESS;
}

bool nchorder_mouse_is_ready(void)
{
    return m_mouse_ready;
}

uint32_t nchorder_mouse_move(int8_t dx, int8_t dy)
{
    if (!m_mouse_ready)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    ret_code_t ret;

    if (dx != 0)
    {
        ret = app_usbd_hid_mouse_x_move(&m_app_hid_mouse, dx);
        if (ret != NRF_SUCCESS && ret != NRF_ERROR_BUSY)
        {
            NRF_LOG_WARNING("Mouse: x_move failed: %d", ret);
            return ret;
        }
    }

    if (dy != 0)
    {
        ret = app_usbd_hid_mouse_y_move(&m_app_hid_mouse, dy);
        if (ret != NRF_SUCCESS && ret != NRF_ERROR_BUSY)
        {
            NRF_LOG_WARNING("Mouse: y_move failed: %d", ret);
            return ret;
        }
    }

    return NRF_SUCCESS;
}

uint32_t nchorder_mouse_scroll(int8_t delta)
{
    if (!m_mouse_ready)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    ret_code_t ret = app_usbd_hid_mouse_scroll_move(&m_app_hid_mouse, delta);
    if (ret != NRF_SUCCESS && ret != NRF_ERROR_BUSY)
    {
        NRF_LOG_WARNING("Mouse: scroll failed: %d", ret);
    }

    return ret;
}

uint32_t nchorder_mouse_button(uint8_t button, bool pressed)
{
    if (!m_mouse_ready)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    if (button >= NCHORDER_MOUSE_BUTTON_COUNT)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    ret_code_t ret = app_usbd_hid_mouse_button_state(&m_app_hid_mouse, button, pressed);
    if (ret != NRF_SUCCESS && ret != NRF_ERROR_BUSY)
    {
        NRF_LOG_WARNING("Mouse: button failed: %d", ret);
    }

    return ret;
}
