// A package's dongle firmware: firmwares/dongle/<board>.zip, one per board (doorman/tools/boards.py zip).
// Each holds board.json (the board, its firmware version), the image its bootloader installs
// (install.uf2 / install.hex, image.bin for the Index radio) and, for a two-bank board, bank-a.bin and
// bank-b.bin: what a self-update writes into the bank that isn't running (lh/dongle_bank.hpp).
#pragma once
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "common/doorman_boards.hpp"
#include "common/nordic_dfu.hpp"
#include "platform/platform.hpp"

namespace lh::dongle_firmware {

namespace fs = std::filesystem;
using doorman_boards::Board;

inline fs::path Zip(const fs::path& dongleDir, const Board& b) { return dongleDir / doorman_boards::ZipName(b); }

inline bool Has(const fs::path& dongleDir, const Board& b) {
  std::error_code ec;
  return fs::exists(Zip(dongleDir, b), ec);
}

// One file from the board's zip.
inline bool Read(const fs::path& dongleDir, const Board& b, const std::string& entry, std::vector<uint8_t>* out) {
  out->clear();
  return Has(dongleDir, b) && lh::dfu::ReadZipEntry(platform::PathUtf8(Zip(dongleDir, b)), entry, out) && !out->empty();
}

// The firmware version the board's zip carries (board.json "firmware": "0x0030"), 0 without one.
inline uint16_t Version(const fs::path& dongleDir, const Board& b) {
  std::vector<uint8_t> json;
  if (!Read(dongleDir, b, "board.json", &json)) return 0;
  const std::string s(json.begin(), json.end());
  const size_t k = s.find("\"firmware\"");
  if (k == std::string::npos) return 0;
  const size_t q = s.find("\"0x", k + 10);
  return q == std::string::npos ? 0 : uint16_t(std::strtoul(s.c_str() + q + 1, nullptr, 16));
}

// The firmware version of the package: every board's zip comes from the same build.
inline uint16_t PackageVersion(const fs::path& dongleDir) {
  for (const auto& b : doorman_boards::kBoards)
    if (!b.indexRadio)
      if (const uint16_t v = Version(dongleDir, b)) return v;
  return 0;
}

// Writes one file from the board's zip to `to`, for the steps that take a file (a UF2 drive copy, a
// .hex loader).
inline bool Extract(const fs::path& dongleDir, const Board& b, const std::string& entry, const fs::path& to) {
  std::vector<uint8_t> data;
  if (!Read(dongleDir, b, entry, &data)) return false;
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
  return bool(out);
}

}  // namespace lh::dongle_firmware
