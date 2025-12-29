/* Linker script for nRF52840 - bare metal (no SoftDevice) */

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x100000
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 0x40000
}

SECTIONS
{
  .text :
  {
    KEEP(*(.isr_vector))
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data :
  {
    *(.data*)
  } > RAM AT > FLASH

  .bss :
  {
    *(.bss*)
    *(COMMON)
  } > RAM

  /* Stack at end of RAM */
  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    . = . + 0x2000;  /* 8KB stack */
    __StackTop = .;
  } > RAM
}

/* Entry point */
ENTRY(Reset_Handler)

/* Provide stack pointer */
__StackLimit = __StackTop - 0x2000;
