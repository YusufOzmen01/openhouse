# Adding devices

Every doorman dongle has a corresponding device descriptor file that is in this folder. You can look at examples here on how you can add support for another device!
- Adafruit (UF2) bootloader for nRF52840: [generic840.json](generic840.json)
- Adafruit (UF2) bootloader for nRF52833: [generic833.json](generic833.json)
- Nordic's open DFU bootloader for nRF52840: [generic840_dfu.json](generic840_dfu.json)
- Nordic's open DFU bootloader for nRF52833: [generic833_dfu.json](generic833_dfu.json)

If the device you're porting can also have a SoftDevice (most Adafruit ones do, check `INFO_UF2.TXT` on its drive), you
need a second file for that build too, like [generic840_sd.json](generic840_sd.json).

## Entries

- `id`: the board id the dongle reports. Pick the next free one. A device that can have a SoftDevice should have a second file with the same id
- `display`: the name shown in the installer and the dongle manager
- `product`: the USB product string
- `note`: a note about the device. Usually it is the type of bootloader it uses
- `soc`: `nrf52840` or `nrf52833`
- `bootloader`: `nordic_dfu` for Nordic's Open DFU, `uf2` for UF2 outputs for nRF52840 and `833_uf2` for UF2 outputs for nRF52833
- `softdevice`: `true` if the bootloader keeps a SoftDevice
- `led`: `{"pin": "PIN_NUMBER", "active_low": ACTIVE_LOW}` PIN_NUMBER is the pin number of the LED, denoted as P0.06 for example. And ACTIVE_LOW is to set it as active low. if set to false, it will be active high. Don't define it if the board doesn't have an LED
- `fem`: only for boards with a front-end amplifier, like the Holyiot. `ctx` and `crx` pins and the adio's `txpower`. Don't define it if your dongle doesn't have it
- `regout0_3v0`: `true` if the board runs straight off USB and its LED is dim (like the PCA10059)
- `pair_page`: where pairings are kept. It has to be below the bootloader. Copy it from the example
- `dfu_by_pads`: `true` if the bootloader can only be entered from pads on the board (like the Kuroshin)

## Building

In `doorman/dongle-fw`, run `make`. Your board's zip is in `build/zips/`. Then run `python3 doorman/tools/boards.py cpp src/common/doorman_boards.hpp` so the apps are also updated with it!
