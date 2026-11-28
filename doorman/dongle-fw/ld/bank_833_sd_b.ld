/* Firmware image for bank B, layout "833_sd" (src/banks.h; nRF52833 with S140 7.3.0 kept, up to 0x27000):
   after the bank's 4 KB header page. Started by the boot stub. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00049000, LENGTH = 0x0005E000 - 0x00049000
  RAM   (rwx) : ORIGIN = 0x20003400, LENGTH = 0x1E000 - 0x3400  /* below: MBR/SoftDevice RAM (untouched). 0x2001E000+: crash record */
}
INCLUDE "nrf_common.ld"
