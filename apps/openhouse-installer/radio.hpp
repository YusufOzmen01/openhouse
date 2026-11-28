// Trackers' and controllers' radio firmware: which devices take which image, finding them
// (wired, behind a Valve/HTC Watchman dongle, behind a Doorman Dongle), and flashing them.
//
// The table comes from SteamVR's own firmware manifests (drivers/*/resources/firmware/
// manifest.vrfirmware): each device's hardware id (feature report 0x05, bytes 41-44) with the
// arguments SteamVR's lighthouse_watchman_update takes for its radio. Every 2.0-era device
// (nRF52 radio) takes the same image with --target=nrf52; the Vive Tracker 1.0 (nRF51) its own
// with -rnt, the Vive wand (nRF51 too) with -rn from its bootloader. Anything else (headsets,
// base stations) is never flashed here.
#pragma once
#include <cstring>
#include "common/doorman_boards.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "system.hpp"

namespace openhouse::installer {

// Closing: a background scan (ScanThread set on its thread) gives up its slow device requests at
// once, so the window doesn't wait for it. Jobs (flashing) aren't scans and always finish.
inline std::atomic<bool> g_closing{false};
inline thread_local bool t_scan = false;
inline bool ScanStopping() { return t_scan && g_closing; }

// Newer: nRF52 radios (doorman-radio-wm3.fw). Tracker10: HTC's nRF51 "vrc module" radios, in the
// VIVE Tracker 1.0 and the VIVE wand (doorman-radio-neo.bin).
enum class RadioFamily { Newer, Tracker10, Unsupported };

struct RadioModel {
  uint32_t hwid = 0;
  std::string name;
  RadioFamily family = RadioFamily::Unsupported;
  bool known = false;  // false: a device openhouse has never seen (the user reports its id)
};
RadioModel ModelFor(uint32_t hwid);

// openhouse's images (the package's firmwares/ folder), and SteamVR's stock one for a device.
fs::path FirmwareFolder();
fs::path OurRadioImage(RadioFamily family);
fs::path StockRadioImage(const fs::path& steamvr, uint32_t hwid);  // "" if SteamVR has none for it
// Every device SteamVR's manifests have a radio file for that openhouse's radio replaces.
struct StockModel {
  uint32_t hwid = 0;
  std::string name;
  RadioFamily family = RadioFamily::Unsupported;
  fs::path image;
};
std::vector<StockModel> StockModels(const fs::path& steamvr);

// A radio app image ready to send: the file's image without a SteamVR .fw trailer.
struct RadioImage {
  std::string data;
  RadioFamily family = RadioFamily::Unsupported;
  uint32_t version = 0;
};
bool LoadRadioImage(const fs::path& file, RadioImage* image, std::string* error);
// Doorman radio versions are 0x6A....... (wm3) and 0x0A....... (neo); stock ones are build times.
bool IsDoormanRadio(uint32_t version);
uint32_t OurRadioVersion(RadioFamily family);  // the packaged image's, 0 if missing

using Progress = std::function<void(float, const std::string&)>;  // 0..1, and what's happening

// ---- wired (USB cable): SteamVR's lighthouse_watchman_update ----
struct WiredDevice {
  std::string serial;  // LHR-...
  uint16_t pid = 0;
  RadioModel model;
  uint32_t radioVersion = 0;  // not readable over USB: 0
};
std::vector<WiredDevice> FindWiredDevices();
fs::path WatchmanUpdateTool(const fs::path& steamvr);
bool FlashWired(const fs::path& steamvr, const WiredDevice& device, const fs::path& image, const Progress& progress, std::string* error);

// ---- wireless ----
enum class Route { Watchman, Doorman };
struct WirelessDevice {
  Route route = Route::Watchman;
  std::string dongle;      // the dongle's serial
  std::string donglePath;  // its HID path
  int slot = -1;           // Doorman Dongle slot
  std::string serial;      // the device's (LHR-...), when known
  RadioModel model;
  uint32_t radioVersion = 0;
  std::string Key() const { return dongle + "/" + std::to_string(slot) + "/" + serial; }
};
std::vector<WirelessDevice> FindWirelessDevices();
// Sends the image, has the radio check and keep it, then powers the device off: it installs the
// new radio the next time it's turned on.
bool FlashWireless(const WirelessDevice& device, const RadioImage& image, const Progress& progress, std::string* error);

// ---- Doorman Dongles ----
struct DoormanDongleInfo {
  std::string serial, path;
  uint16_t firmware = 0;
  bool softDeviceBuild = false;  // running the "_sd" build
  int board = 0;                 // 0 UF2 (Pro Micro style), 1 Holyiot 21017, 2 PCA10059, 3 Kuroshin (Nordic DFU),
                                 // 4 Valve Index HMD radio (Valve's bootloader)
  int bank = 0;                  // the bank it runs from (1 A, 2 B); 0: older firmware without banks
  int slots = 8;                 // the slots it has (it says so from firmware 0x002A; older ones have 8)
  int oldRadios = -1;            // its connected devices whose radio has an update (-1: not asked)
  const lh::doorman_boards::Board* Board() const { return lh::doorman_boards::Find(board, softDeviceBuild); }
  bool NordicDfu() const { const auto* b = Board(); return b && std::strcmp(b->bootloader, "nordic_dfu") == 0; }
  bool Banked() const { return bank != 0; }   // updates itself over USB, no bootloader
  bool IndexRadio() const { const auto* b = Board(); return b && b->indexRadio; }
};
std::vector<DoormanDongleInfo> FindDoormanDongles();
bool RebootToBootloader(const DoormanDongleInfo& d);  // its UF2 drive appears (Nordic DFU boards: its serial port)
// One slot of a Doorman Dongle: paired or not, its device on or not, and the device (radio version,
// model) when it's on and answers.
struct DongleSlot {
  int slot = 0;
  bool paired = false, on = false, answered = false;
  WirelessDevice device;
  uint32_t storedRadio = 0;   // the radio version the dongle remembers for it, off too (fw 0x002C+; 0 unknown)
};
std::vector<DongleSlot> ScanDongleSlots(const DoormanDongleInfo& dongle);   // every slot it has
// Closes the device handles kept open between scans (the radio pages' Watchman dongles and wired
// devices), all but the Doorman Dongles' that the home page keeps checking. Going home.
void ReleaseDeviceHandles();
// The radio version a Doorman Dongle remembers for a slot (firmware 0x002D+; false before).
bool SetRememberedRadio(const std::string& donglePath, int slot, uint32_t version);
// Turns a device on a Doorman Dongle off (its main MCU's power-off command, as the driver sends it).
void PowerOffDevice(const WirelessDevice& device);

// ---- the Valve Index HMD's radio as a Doorman dongle ----
// The headset has two radios (28DE:2102); one is flashed, the other is left to SteamVR.
std::vector<std::string> FindIndexRadios();               // stock radios' USB serials
std::string PickIndexRadio(const std::vector<std::string>& serials);   // the one to flash ("" none)
// SteamVR's newest Index radio package (its signed init packet is the reference), "" none.
std::string IndexRadioPackage(const std::string& steamvrRoot);
// Flashes Doorman onto an Index radio (image "": SteamVR's stock radio firmware out of the package,
// for a restore; either way signed at the version the bootloader holds). enterDfu puts it in Valve's bootloader (a stock radio: the
// HID request; ours: cmd 0x10); serial: its USB serial, "" to take the bootloader that appears.
bool FlashIndexRadio(const std::string& serial, const std::function<bool()>& enterDfu, const std::string& image,
                     const std::string& package, const Progress& progress, std::string* error);
bool TriggerIndexRadioDfu(const std::string& serial);    // a stock radio into its bootloader

// A banked dongle writes `image` (the firmware for the bank it isn't running) and restarts into it.
bool BankUpdate(const DoormanDongleInfo& d, std::vector<uint8_t> image, const std::function<void(float)>& progress,
                std::string* error);

}  // namespace openhouse::installer
