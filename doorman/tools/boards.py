#!/usr/bin/env python3
"""Doorman dongle boards: one file per board in doorman/boards/*.json, and what's made from them.

  boards.py header NAME OUT.h   the firmware's board header (dongle-fw: built with -include)
  boards.py mk OUT.mk           the dongle-fw Makefile's board list
  boards.py cpp OUT.hpp         the apps' board table (src/common/doorman_boards.hpp, kept in the repo)
  boards.py check-cpp FILE      fails when FILE isn't what `cpp` makes now
  boards.py zip NAME BUILD OUT   the board's zip from the dongle-fw build folder (firmwares/dongle/<name>.zip)
  boards.py list                the boards, one per line

A board file (see doorman/boards/ADDING_DEVICES.md), named after the board (<name>.json: its builds are
build/doorman_dongle_<name>*, its zip <name>.zip):
  id          cmd 0x01 byte 57; boards sharing one (the Pro Micro's two builds) tell apart by softdevice
  display     name shown in the apps; note: the line under it
  product     the USB product string, after "Doorman " (the apps find dongles by that prefix)
              Ids 0-4 are openhouse's own dongles (the installer's tiles); the rest are listed under
              Other Dongles, by id.
  soc         nrf52840 | nrf52833
  softdevice  its bootloader keeps a SoftDevice (the app starts at 0x26000). The flash layout built
              (banks.h, ld/) follows from it, the SoC and the kind: sd with one, std without, the nRF52833's
              own addresses (833_uf2), the Index radio's single image (idx).
  bootloader  uf2 (Adafruit) | nordic_dfu (Nordic's open USB DFU) | valve_serial (Valve's radio DFU)
  led         {"pin": "P0.06", "active_low": true}, or left out: a board whose LED isn't known drives no pin
  fem         {"ctx": "P0.24", "crx": "P0.22", "txpower": "Neg4dBm"}: a SKY66112-style front-end amplifier
  regout0_3v0 sets the GPIO supply to 3.0 V once (UICR REGOUT0), for a board running off VDDH
  pair_page   the flash page that keeps pairings
  kind        dongle (default) | index_radio | check (built in CI, never shipped)
  dfu_by_pads its bootloader only starts from its pads
"""
import json
import pathlib
import re
import sys
import zipfile

HERE = pathlib.Path(__file__).resolve().parent
BOARDS = HERE.parent / 'boards'
GPREGRET = {'uf2': (0x57, 0x4E), 'nordic_dfu': (0xB1, 0x4E), 'valve_serial': (0xB1, 0xB1)}
UF2_FAMILY = {'nrf52840': '0xADA52840', 'nrf52833': '0x621E937A'}


def load():
    boards = []
    for p in sorted(BOARDS.glob('*.json')):
        b = json.loads(p.read_text())
        if 'name' in b:
            sys.exit(f'boards: {p.name}: no "name": the file name is the board\'s name')
        b = {'name': p.stem, **b}
        b.setdefault('kind', 'dongle')
        b.setdefault('led', None)
        b.setdefault('fem', None)
        if 'layout' in b:
            sys.exit(f'boards: {p.name}: no "layout": it follows from "softdevice"')
        b.setdefault('softdevice', False)
        if b['soc'] == 'nrf52833' and b['softdevice']:
            sys.exit(f'boards: {p.name}: no flash layout for an nRF52833 with a SoftDevice yet')
        # the flash layout built (banks.h, ld/): the Index radio's single image, the nRF52833's own, sd or std
        b['flash'] = ('idx' if b['kind'] == 'index_radio' else '833_uf2' if b['soc'] == 'nrf52833'
                      else 'sd' if b['softdevice'] else 'std')
        boards.append(b)
    ids = {}
    for b in boards:
        if b['kind'] == 'check':
            continue
        key = (b['id'], b['softdevice'])
        if key in ids:
            sys.exit(f'boards: {b["name"]} and {ids[key]} share id {b["id"]} (and softdevice)')
        ids[key] = b['name']
    return boards


def pin(b, text, what):
    """ "P0.06" -> (port, pin); P1 goes to P1.15 on an nRF52840, P1.09 on an nRF52833."""
    m = re.fullmatch(r'P([01])\.(\d{1,2})', str(text))
    top = {0: 31, 1: 15 if b['soc'] == 'nrf52840' else 9}
    if not m or int(m.group(2)) > top[int(m.group(1))]:
        sys.exit(f'boards: {b["name"]}.json: {what} {text!r} isn\'t a pin of the {b["soc"]} (like "P0.06")')
    return int(m.group(1)), int(m.group(2))


def board(name):
    for b in load():
        if b['name'] == name:
            return b
    sys.exit(f'boards: no board {name!r}')


def header(b):
    uf2, serial = GPREGRET[b['bootloader']]
    led, fem = b['led'], b['fem']
    lines = [
        f'/* Board {b["name"]}: made by doorman/tools/boards.py from doorman/boards/{b["name"]}.json. Don\'t edit. */',
        '#pragma once',
        f'#define BOARD_NAME "{b["name"]}"',
        f'#define BOARD_ID {b["id"]}',
        f'#define DONGLE_PRODUCT "Doorman {b["product"]}"',
        f'#define VARIANT_SD {int(b["softdevice"])}',
        f'#define BANK_LAYOUT_SD {int(b["flash"] == "sd")}',
        f'#define BANK_LAYOUT_833_UF2 {int(b["flash"] == "833_uf2")}',
        f'#define BOARD_IDX {int(b["kind"] == "index_radio")}',
        f'#define BOARD_NORDIC_DFU {int(b["bootloader"] != "uf2")}',
        f'#define BOARD_PAIR_PAGE {b["pair_page"]}u',
        f'#define GPREGRET_UF2_DFU 0x{uf2:02X}',
        f'#define GPREGRET_SERIAL_DFU 0x{serial:02X}',
        f'#define BOARD_HAS_LED {int(led is not None)}',
    ]
    if led is not None:
        port, n = pin(b, led['pin'], 'led pin')
        lines += [f'#define BOARD_LED_PORT {port}', f'#define BOARD_LED_PIN {n}',
                  f'#define BOARD_LED_ACTIVE_LOW {int(led["active_low"])}']
    lines.append(f'#define BOARD_HAS_FEM {int(fem is not None)}')
    if fem is not None:   # GPIOTE takes port * 32 + pin
        ctx, crx = pin(b, fem['ctx'], 'fem ctx'), pin(b, fem['crx'], 'fem crx')
        lines += [f'#define BOARD_FEM_CTX_PIN {ctx[0] * 32 + ctx[1]}', f'#define BOARD_FEM_CRX_PIN {crx[0] * 32 + crx[1]}']
    lines.append(f'#define BOARD_RADIO_TXPOWER RADIO_TXPOWER_TXPOWER_{(fem or {}).get("txpower", "Pos8dBm")}')
    lines.append(f'#define BOARD_REGOUT0_3V0 {int(bool(b.get("regout0_3v0")))}')
    return '\n'.join(lines) + '\n'


def mk(boards):
    banked = [b['name'] for b in boards if b['kind'] in ('dongle', 'check')]
    out = ['# made by doorman/tools/boards.py from doorman/boards/*.json. Don\'t edit.',
           'VARIANTS = ' + ' '.join(b['name'] for b in boards if b['kind'] == 'dongle'),
           'CHECKS = ' + ' '.join(b['name'] for b in boards if b['kind'] == 'check'),
           'BANKED = ' + ' '.join(banked)]
    for b in boards:
        out += [f'soc_{b["name"]} = {b["soc"]}', f'layout_{b["name"]} = {b["flash"]}',
                f'bootloader_{b["name"]} = {b["bootloader"]}', f'family_{b["name"]} = {UF2_FAMILY[b["soc"]]}']
    return '\n'.join(out) + '\n'


def cstr(s):
    return 'nullptr' if s is None else json.dumps(s)


def cpp(boards):
    rows = []
    for b in sorted((b for b in boards if b['kind'] != 'check'), key=lambda b: (b['id'], b['softdevice'], b['name'])):
        rows.append('  {%s, %d, %s, %s, %s, %s, %s, %s, %s},' % (
            cstr(b['name']), b['id'], 'true' if b['softdevice'] else 'false', cstr(b['display']), cstr(b.get('note', '')),
            cstr(b['soc']), cstr(b['bootloader']),
            'true' if b['kind'] == 'index_radio' else 'false', 'true' if b.get('dfu_by_pads') else 'false'))
    return f'''// Doorman dongle boards, made by doorman/tools/boards.py from doorman/boards/*.json. Don't edit:
// change the board files and run `python3 doorman/tools/boards.py cpp src/common/doorman_boards.hpp`.
#pragma once
#include <cstring>
#include <string>

namespace lh::doorman_boards {{

struct Board {{
  const char* name;        // build name: firmwares/dongle/<name>.zip
  int id;                  // cmd 0x01 byte 57
  bool softdevice;         // its bootloader keeps a SoftDevice (cmd 0x01 byte 6)
  const char* display;
  const char* note;
  const char* soc;         // "nrf52840" or "nrf52833"
  const char* bootloader;  // "uf2", "nordic_dfu" or "valve_serial"
  bool indexRadio;
  bool dfuByPads;
}};

// Ids up to this are openhouse's own dongles (the installer's tiles); the others are listed under Other Dongles.
inline constexpr int kLastOwnId = 4;
inline bool Other(const Board& b) {{ return b.id > kLastOwnId; }}

inline constexpr Board kBoards[] = {{
{chr(10).join(rows)}
}};

// The board a dongle runs: by its id, and for boards sharing one, whether its bootloader keeps a SoftDevice.
inline const Board* Find(int id, bool softdevice) {{
  const Board* any = nullptr;
  for (const auto& b : kBoards)
    if (b.id == id) {{
      if (b.softdevice == softdevice) return &b;
      if (!any) any = &b;
    }}
  return any;
}}

inline const Board* Named(const std::string& name) {{
  for (const auto& b : kBoards)
    if (name == b.name) return &b;
  return nullptr;
}}

// Its zip in a package (firmwares/dongle/<name>.zip) and the files in it.
inline std::string ZipName(const Board& b) {{ return std::string(b.name) + ".zip"; }}
inline const char* InstallEntry(const Board& b) {{
  return std::strcmp(b.bootloader, "uf2") == 0 ? "install.uf2" : std::strcmp(b.bootloader, "valve_serial") == 0 ? "image.bin" : "install.hex";
}}
// The bank image to send for a self-update: the bank that isn't running (runningBank: 1 A, 2 B).
inline const char* BankEntry(int runningBank) {{ return runningBank == 1 ? "bank-b.bin" : "bank-a.bin"; }}

}}  // namespace lh::doorman_boards
'''


def firmware_version():
    text = (HERE.parent / 'common' / 'doorman_versions.h').read_text()
    return int(re.search(r'#define\s+DONGLE_FW_VERSION\s+(0x[0-9A-Fa-f]+)', text).group(1), 16)


def install_entry(b):
    return {'uf2': 'install.uf2', 'valve_serial': 'image.bin'}.get(b['bootloader'], 'install.hex')


def make_zip(b, build, out):
    """One board's firmware: board.json (the board file, the firmware version and what's inside), the
    install image its bootloader takes, and for a two-bank board the images a self-update writes."""
    build = pathlib.Path(build)
    stem = f'doorman_dongle_{b["name"]}'
    files = {}
    if b['kind'] == 'index_radio':
        files['image.bin'] = build / f'{stem}.bin'
    else:
        files[install_entry(b)] = build / (f'{stem}.uf2' if b['bootloader'] == 'uf2' else f'{stem}.hex')
        files['bank-a.bin'] = build / f'{stem}_a.bin'
        files['bank-b.bin'] = build / f'{stem}_b.bin'
    meta = dict(b, firmware=f'0x{firmware_version():04X}', files=sorted(files))
    entries = [('board.json', (json.dumps(meta, indent=2) + '\n').encode())]
    entries += [(name, path.read_bytes()) for name, path in sorted(files.items())]
    tmp = pathlib.Path(str(out) + '.tmp')
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in entries:
            info = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))   # fixed: the same build, the same zip
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    tmp.replace(out)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'header':
        out = header(board(sys.argv[2]))
        p = pathlib.Path(sys.argv[3])
        if not p.exists() or p.read_text() != out:
            p.write_text(out)
    elif cmd == 'mk':
        p = pathlib.Path(sys.argv[2])
        out = mk(load())
        if not p.exists() or p.read_text() != out:
            p.write_text(out)
    elif cmd == 'cpp':
        pathlib.Path(sys.argv[2]).write_text(cpp(load()))
    elif cmd == 'check-cpp':
        if pathlib.Path(sys.argv[2]).read_text() != cpp(load()):
            sys.exit(f'boards: {sys.argv[2]} is out of date: python3 doorman/tools/boards.py cpp {sys.argv[2]}')
    elif cmd == 'zip':
        make_zip(board(sys.argv[2]), sys.argv[3], sys.argv[4])
    elif cmd == 'list':
        for b in load():
            print(b['name'], b['id'], b['kind'], b['bootloader'], b['soc'], 'softdevice' if b['softdevice'] else '-', b['flash'])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
