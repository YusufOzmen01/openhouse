// Doorman dongle boards, made by doorman/tools/boards.py from doorman/boards/*.json. Don't edit:
// change the board files and run `python3 doorman/tools/boards.py cpp src/common/doorman_boards.hpp`.
#pragma once
#include <cstring>
#include <string>

namespace lh::doorman_boards {

struct Board {
  const char* name;        // build name: firmwares/dongle/<name>.zip
  int id;                  // cmd 0x01 byte 57
  bool softdevice;         // its bootloader keeps a SoftDevice (cmd 0x01 byte 6)
  const char* display;
  const char* note;
  const char* soc;         // "nrf52840" or "nrf52833"
  const char* bootloader;  // "uf2", "nordic_dfu" or "valve_serial"
  bool indexRadio;
  bool dfuByPads;
};

// Ids up to this are openhouse's own dongles (the installer's tiles); the others are listed under Other Dongles.
inline constexpr int kLastOwnId = 4;
inline bool Other(const Board& b) { return b.id > kLastOwnId; }

inline constexpr Board kBoards[] = {
  {"nosd", 0, false, "ProMicro nRF52840", "Adafruit bootloader. Soldering an antenna to the board is highly recommended", "nrf52840", "uf2", false, false},
  {"sd", 0, true, "ProMicro nRF52840", "Adafruit bootloader. Soldering an antenna to the board is highly recommended", "nrf52840", "uf2", false, false},
  {"holyiot", 1, false, "Holyiot 21017", "Nordic bootloader", "nrf52840", "nordic_dfu", false, false},
  {"pca10059", 2, false, "Nordic PCA10059 Dongle", "Nordic bootloader", "nrf52840", "nordic_dfu", false, false},
  {"kuroshin", 3, false, "Kuroshin", "It needs to be put into DFU mode by opening the dongle and shorting two pins", "nrf52840", "nordic_dfu", false, true},
  {"idx", 4, true, "Valve Index", "The headset's own radio. Index only", "nrf52840", "valve_serial", true, false},
  {"generic840", 5, false, "Generic nRF52840", "Adafruit bootloader", "nrf52840", "uf2", false, false},
  {"generic840_sd", 5, true, "Generic nRF52840", "Adafruit bootloader", "nrf52840", "uf2", false, false},
  {"generic833", 6, false, "Generic nRF52833", "Adafruit bootloader", "nrf52833", "uf2", false, false},
  {"generic840_dfu", 7, false, "Generic nRF52840", "Nordic bootloader", "nrf52840", "nordic_dfu", false, false},
  {"generic833_dfu", 8, false, "Generic nRF52833", "Nordic bootloader", "nrf52833", "nordic_dfu", false, false},
};

// The board a dongle runs: by its id, and for boards sharing one, whether its bootloader keeps a SoftDevice.
inline const Board* Find(int id, bool softdevice) {
  const Board* any = nullptr;
  for (const auto& b : kBoards)
    if (b.id == id) {
      if (b.softdevice == softdevice) return &b;
      if (!any) any = &b;
    }
  return any;
}

inline const Board* Named(const std::string& name) {
  for (const auto& b : kBoards)
    if (name == b.name) return &b;
  return nullptr;
}

// Its zip in a package (firmwares/dongle/<name>.zip) and the files in it.
inline std::string ZipName(const Board& b) { return std::string(b.name) + ".zip"; }
inline const char* InstallEntry(const Board& b) {
  return std::strcmp(b.bootloader, "uf2") == 0 ? "install.uf2" : std::strcmp(b.bootloader, "valve_serial") == 0 ? "image.bin" : "install.hex";
}
// The bank image to send for a self-update: the bank that isn't running (runningBank: 1 A, 2 B).
inline const char* BankEntry(int runningBank) { return runningBank == 1 ? "bank-b.bin" : "bank-a.bin"; }

}  // namespace lh::doorman_boards
