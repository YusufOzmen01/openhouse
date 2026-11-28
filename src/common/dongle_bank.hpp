// Doorman dongle self-update (doorman/dongle-fw/src/banks.h): a running dongle writes the firmware for
// the bank it isn't running into that bank and restarts into it. Commands 0x40 (begin: size, CRC; erases),
// 0x41 (write: offset, n, bytes), 0x42 (commit: checks the CRC, writes the bank header, restarts).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lh::dongle_bank {

constexpr uint8_t kBegin = 0x40, kWrite = 0x41, kCommit = 0x42;
constexpr size_t kChunk = 56;

inline uint32_t Crc32(const std::vector<uint8_t>& data) {
  uint32_t c = 0xFFFFFFFFu;
  for (uint8_t b : data) {
    c ^= b;
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}


}  // namespace lh::dongle_bank
