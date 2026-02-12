/* Linker script for GPIO test - Runs after SoftDevice S140 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Start after SoftDevice S140 so --sectorerase preserves it */
  FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0xc9000
  RAM (rwx) :  ORIGIN = 0x20003000, LENGTH = 0x3d000
  UICR_REGOUT0 (r) : ORIGIN = 0x10001304, LENGTH = 0x4
  UICR_NFCPINS (r) : ORIGIN = 0x1000120C, LENGTH = 0x4
}

SECTIONS
{
  .uicr_regout0 :
  {
    KEEP(*(.uicr_regout0))
  } > UICR_REGOUT0
  .uicr_nfcpins :
  {
    KEEP(*(.uicr_nfcpins))
  } > UICR_NFCPINS
}

INCLUDE "nrf_common.ld"
