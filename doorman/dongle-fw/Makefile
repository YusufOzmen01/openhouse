# Doorman dongle firmware (ai/doorman/dongle-fw.md)
# make            -> every board's install image and bank images (below)
# make idx       -> the Valve Index HMD radio image (ai/doorman/index-radio.md)
# make check-nrf52833 -> the nRF52833 boards only
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
TUSB = ../third_party/tinyusb
NRFX = ../third_party/nrfx
CFLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Os -flto -g -ffunction-sections -fdata-sections \
  -Wall -Wextra -Wno-unused-parameter -std=gnu11 -DCFG_TUSB_MCU=OPT_MCU_NRF5X \
  -Isrc -I../common -I$(TUSB)/src -I$(NRFX) -I$(NRFX)/mdk -I$(NRFX)/hal -I$(NRFX)/drivers/include -I$(NRFX)/drivers/src -I$(NRFX)/templates \
  -I../third_party/CMSIS_5/CMSIS/Core/Include -D__STACK_SIZE=4096 -D__HEAP_SIZE=0
sources = src/main.c src/link.c src/usb_descriptors.c $(NRFX)/mdk/system_$(1).c \
  $(TUSB)/src/tusb.c $(TUSB)/src/common/tusb_fifo.c $(TUSB)/src/device/usbd.c $(TUSB)/src/device/usbd_control.c \
  $(TUSB)/src/class/hid/hid_device.c $(TUSB)/src/portable/nordic/nrf5x/dcd_nrf5x.c
LDFLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -flto -Wl,--gc-sections -L$(NRFX)/mdk -specs=nano.specs -specs=nosys.specs

# Every board runs the two-bank layout (src/banks.h, ai/doorman/dongle-fw.md):
#   build/doorman_dongle_<v>_a.bin / _b.bin   firmware for bank A / B: self-updates over USB (cmd 0x40-0x42)
#   build/doorman_dongle_<v>.hex (.uf2)       boot stub + bank A's image: what a bootloader installs
#                                             (first install or rescue)
# The boards: one file each in ../boards/*.json (ai/doorman/boards.md). ../tools/boards.py makes the board
# list (build/boards.mk: VARIANTS, the shipped dongles; CHECKS, built but never shipped) and each board's
# header (build/board_<name>.h, passed with -include): its SoC, flash layout, bootloader and hardware.
BOARD_FILES = $(wildcard ../boards/*.json)
build/boards.mk: $(BOARD_FILES) ../tools/boards.py
	@mkdir -p build
	python3 ../tools/boards.py mk $@
-include build/boards.mk
build/board_%.h: ../boards/%.json ../tools/boards.py
	@mkdir -p build
	python3 ../tools/boards.py header $* $@
soc = $(soc_$(1))
device_nrf52840 = NRF52840_XXAA
device_nrf52833 = NRF52833_XXAA
layout = $(layout_$(1))
startup = $(NRFX)/mdk/gcc_startup_$(call soc,$(1)).S
chip_flags = -D$(device_$(call soc,$(1)))
board_flags = -include build/board_$(1).h

all: $(foreach v,$(VARIANTS),build/doorman_dongle_$(v).hex build/doorman_dongle_$(v)_a.bin build/doorman_dongle_$(v)_b.bin) \
  $(foreach v,$(VARIANTS),$(if $(filter uf2,$(bootloader_$(v))),build/doorman_dongle_$(v).uf2)) build/doorman_dongle_idx.bin \
  zips

define variant
build/doorman_dongle_$(1).uf2: UF2_FAMILY = $(family_$(1))
build/doorman_dongle_$(1)_%.elf: Makefile build/board_$(1).h $(call sources,$(call soc,$(1))) $(call startup,$(1)) src/*.h ../common/*.h $(NRFX)/mdk/nrf_common.ld ld/bank_$(call layout,$(1))_%.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(call chip_flags,$(1)) $(call board_flags,$(1)) $(call sources,$(call soc,$(1))) $(call startup,$(1)) \
	  $(LDFLAGS) -Tld/bank_$(call layout,$(1))_$$*.ld -Wl,-Map=build/doorman_dongle_$(1)_$$*.map -o $$@
	arm-none-eabi-size $$@

# the boot stub, with bank A's image packed as payload (src/lz_unpack.h)
build/doorman_dongle_$(1).elf: Makefile build/board_$(1).h src/stub.c src/banks.h src/dongle.h src/lz_unpack.h ld/stub_$(call layout,$(1)).ld build/doorman_dongle_$(1)_a.lz
	$(CC) $(CFLAGS) $(call chip_flags,$(1)) $(call board_flags,$(1)) -DPAYLOAD_BIN='"build/doorman_dongle_$(1)_a.lz"' src/stub.c -nostartfiles -nostdlib \
	  $(LDFLAGS) -Tld/stub_$(call layout,$(1)).ld -Wl,-Map=build/doorman_dongle_$(1).map -o $$@
	arm-none-eabi-size $$@
endef
$(foreach v,$(BANKED),$(eval $(call variant,$(v))))

# One zip per shipped board (and the Index radio): build/zips/<name>.zip, what the packages carry in
# firmwares/dongle/ (../tools/boards.py zip: board.json, the install image, the two bank images).
build/zips/%.zip: ../boards/%.json ../tools/boards.py ../common/doorman_versions.h
	@mkdir -p build/zips
	python3 ../tools/boards.py zip $* build $@
$(foreach v,$(VARIANTS),$(eval build/zips/$(v).zip: build/doorman_dongle_$(v).hex build/doorman_dongle_$(v)_a.bin build/doorman_dongle_$(v)_b.bin $(if $(filter uf2,$(bootloader_$(v))),build/doorman_dongle_$(v).uf2)))
build/zips/idx.zip: build/doorman_dongle_idx.bin
zips: $(foreach v,$(VARIANTS),build/zips/$(v).zip) build/zips/idx.zip

# The nRF52833 builds (the generic boards): both banks and the install image.
check-nrf52833: build/zips/generic833.zip build/zips/generic833_dfu.zip

# Valve Index HMD radio ("gravidongle", ai/doorman/index-radio.md): nRF52840 + S140 6.1.1, one image at 0x26000
# updated through Valve's serial-DFU bootloader; no banks, no stub.
build/doorman_dongle_idx.elf: Makefile build/board_idx.h $(call sources,nrf52840) $(call startup,idx) src/*.h ../common/*.h $(NRFX)/mdk/nrf_common.ld ld/app_idx.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(call chip_flags,idx) $(call board_flags,idx) $(call sources,nrf52840) $(call startup,idx) \
	  $(LDFLAGS) -Tld/app_idx.ld -Wl,-Map=build/doorman_dongle_idx.map -o $@
	arm-none-eabi-size $@
build/doorman_dongle_idx.bin: build/doorman_dongle_idx.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s %4 $@
idx: build/doorman_dongle_idx.bin

# a bank image as raw bytes from its first address, padded to whole words
build/%_a.bin: build/%_a.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s %4 $@
build/%_b.bin: build/%_b.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s %4 $@
# bank A's image packed for the install image, checked by unpacking it with the stub's own code on this host
HOSTCC ?= cc
build/lzcheck: tools/lzcheck.c src/lz_unpack.h
	@mkdir -p build
	$(HOSTCC) -O2 -Wall -Isrc -o $@ $<
build/%_a.lz: build/%_a.bin tools/lzpack.py build/lzcheck
	python3 tools/lzpack.py $< $@
	build/lzcheck $@ $<
build/%.hex: build/%.elf
	$(OBJCOPY) -O ihex $< $@
UF2_FAMILY ?= 0xADA52840
build/%.uf2: build/%.hex
	python3 tools/uf2conv.py -f $(UF2_FAMILY) -c -o $@ $< >/dev/null

clean:
	rm -rf build
.SECONDARY:
.PHONY: all clean check-nrf52833 idx zips
