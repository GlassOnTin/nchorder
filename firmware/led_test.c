/**
 * Bare-metal LED test - tries BOTH XIAO and DK LED pins
 */

#include <stdint.h>

// nRF52840 GPIO registers
#define GPIO_BASE       0x50000000UL
#define GPIO_OUTSET     (*(volatile uint32_t *)(GPIO_BASE + 0x508))
#define GPIO_OUTCLR     (*(volatile uint32_t *)(GPIO_BASE + 0x50C))
#define GPIO_DIRSET     (*(volatile uint32_t *)(GPIO_BASE + 0x518))
#define GPIO_PIN_CNF(n) (*(volatile uint32_t *)(GPIO_BASE + 0x700 + (n) * 4))

// XIAO nRF52840 LED pins
#define XIAO_LED_RED   26
#define XIAO_LED_GREEN 30
#define XIAO_LED_BLUE  6

// nRF52840-DK LED pins (active low)
#define DK_LED1  13
#define DK_LED2  14
#define DK_LED3  15
#define DK_LED4  16

// nRF5340-DK LED pins (active low)
#define DK5340_LED1  28
#define DK5340_LED2  29
#define DK5340_LED3  30
#define DK5340_LED4  31

static void delay_ms(uint32_t ms)
{
    volatile uint32_t count = ms * 8000;
    while (count--) {
        __asm__("nop");
    }
}

static void gpio_cfg_output(uint32_t pin)
{
    GPIO_PIN_CNF(pin) = 1;
    GPIO_DIRSET = (1UL << pin);
}

static void gpio_set(uint32_t pin)
{
    GPIO_OUTSET = (1UL << pin);
}

static void gpio_clear(uint32_t pin)
{
    GPIO_OUTCLR = (1UL << pin);
}

int main(void)
{
    // Configure ALL possible LED pins
    gpio_cfg_output(XIAO_LED_RED);
    gpio_cfg_output(XIAO_LED_GREEN);
    gpio_cfg_output(XIAO_LED_BLUE);
    gpio_cfg_output(DK_LED1);
    gpio_cfg_output(DK_LED2);
    gpio_cfg_output(DK_LED3);
    gpio_cfg_output(DK_LED4);
    gpio_cfg_output(DK5340_LED1);
    gpio_cfg_output(DK5340_LED2);
    gpio_cfg_output(DK5340_LED3);
    gpio_cfg_output(DK5340_LED4);

    // All OFF (set = off for active low)
    gpio_set(XIAO_LED_RED);
    gpio_set(XIAO_LED_GREEN);
    gpio_set(XIAO_LED_BLUE);
    gpio_set(DK_LED1);
    gpio_set(DK_LED2);
    gpio_set(DK_LED3);
    gpio_set(DK_LED4);
    gpio_set(DK5340_LED1);
    gpio_set(DK5340_LED2);
    gpio_set(DK5340_LED3);
    gpio_set(DK5340_LED4);

    delay_ms(500);

    while (1)
    {
        // Blink ALL possible LEDs with CLEAR (should turn ON if active low)
        gpio_clear(XIAO_LED_RED);
        gpio_clear(XIAO_LED_GREEN);
        gpio_clear(XIAO_LED_BLUE);
        gpio_clear(DK_LED1);
        gpio_clear(DK_LED2);
        gpio_clear(DK_LED3);
        gpio_clear(DK_LED4);
        gpio_clear(DK5340_LED1);
        gpio_clear(DK5340_LED2);
        gpio_clear(DK5340_LED3);
        gpio_clear(DK5340_LED4);

        delay_ms(500);

        // All OFF
        gpio_set(XIAO_LED_RED);
        gpio_set(XIAO_LED_GREEN);
        gpio_set(XIAO_LED_BLUE);
        gpio_set(DK_LED1);
        gpio_set(DK_LED2);
        gpio_set(DK_LED3);
        gpio_set(DK_LED4);
        gpio_set(DK5340_LED1);
        gpio_set(DK5340_LED2);
        gpio_set(DK5340_LED3);
        gpio_set(DK5340_LED4);

        delay_ms(500);
    }
}

__attribute__((section(".isr_vector")))
const void *vector_table[] = {
    (void *)0x20040000,
    main,
};
