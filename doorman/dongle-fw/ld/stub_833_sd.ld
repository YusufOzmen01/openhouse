/* Boot stub, layout "833_sd" (src/stub.c, src/banks.h; nRF52833 whose bootloader keeps S140 7.3.0: the SoftDevice
   area (0x1000-0x26FFF) is left alone): at the app start, followed by its payload (bank A's image). Must end below
   bank A: the linker checks it. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00027000, LENGTH = 0x00032000 - 0x00027000
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x1E000
}
ENTRY(stub_main)
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) } > FLASH
  .payload : { KEEP(*(.payload)) } > FLASH
  /DISCARD/ : { *(.data*) *(.bss*) *(.ARM.*) }
  __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
