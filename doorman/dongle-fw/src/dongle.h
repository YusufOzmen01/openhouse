#pragma once
#include "nrf.h"
#include "doorman_versions.h"   /* DONGLE_FW_VERSION */

#define DONGLE_VID         0x6767
#define DONGLE_PID         0x0727
/* USB data interfaces: slot s streams on interface s % DONGLE_NIF (interface 0 also carries commands).
   Five since 0x002B (four before): hosts take the count from bcdDevice. */
#define DONGLE_NIF         5
#define STREAM_KIND        0x80   /* IN report: [0x80][slot][n][n bytes of [len][frame]... stream] */

/* host <-> dongle, 64-byte HID reports (no report ID)
   OUT: [cmd][seq][args...]      IN: [cmd][seq][status][data...]   status 0 = ok */
enum {
    CMD_INFO = 0x01, CMD_STATS = 0x02, CMD_ECHO = 0x03,
    CMD_DFU_UF2 = 0x10, CMD_DFU_SERIAL = 0x11, CMD_RESET = 0x12, CMD_CRASH_TEST = 0x13, CMD_CLEAR_CRASH = 0x14,
    CMD_LINK = 0x20, CMD_DOWN = 0x21, CMD_RADIO_RSSI_SCAN = 0x22, CMD_LINKSTATS = 0x23,
    /* self-update (banks.h): [size u32][crc u32] erases the other bank; [offset u32][n][n bytes] writes
       into it; commit checks the CRC, writes its header (next sequence number) and restarts into it */
    CMD_BANK_BEGIN = 0x40, CMD_BANK_WRITE = 0x41, CMD_BANK_COMMIT = 0x42,
};
enum { ST_OK = 0, ST_BAD_CMD = 1, ST_BAD_ARG = 2, ST_TIMEOUT = 3 };

/* The board: build/board_<name>.h, made from doorman/boards/<name>.json (doorman/tools/boards.py) and
   passed with -include. It names the board (BOARD_ID: cmd 0x01 byte 57, DONGLE_PRODUCT: the USB product
   string, which host code matches by its "Doorman " prefix), its flash layout, its bootloader's GPREGRET
   values (GPREGRET_UF2_DFU, GPREGRET_SERIAL_DFU) and its hardware (BOARD_HAS_LED, BOARD_HAS_FEM, ...). */
#ifndef BOARD_NAME
#error "no board header: build through the Makefile (-include build/board_<name>.h)"
#endif
