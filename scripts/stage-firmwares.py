#!/usr/bin/env python3
"""Builds openhouse's firmware and puts it in a package's firmwares/ folder, where
openhouse-installer looks for it:

  firmwares/basestation/lhbs-<date>-<commit>.bin   1.0 base stations
  firmwares/dongle/<board>.zip                     one per Doorman dongle board (doorman/boards/<board>.json) and the
                                                   Index radio: board.json (the board and its firmware version), the
                                                   image its bootloader installs (install.uf2 / install.hex, image.bin
                                                   for the Index radio) and bank-a.bin / bank-b.bin, what a running
                                                   dongle updates itself with (doorman/tools/boards.py zip)
  firmwares/radio/doorman-radio-wm3.fw             every 2.0-era device (nRF52 radio)
  firmwares/radio/doorman-radio-neo.bin            VIVE Tracker 1.0 (nRF51 radio)

usage: stage-firmwares.py PACKAGE_DIR      (needs arm-none-eabi-gcc with newlib)
With OPENHOUSE_FIRMWARES set to a firmwares/ folder already built (CI's firmware job, scripts/build-all.sh), that
folder is copied instead: every package then carries the same images.
"""
import datetime
import shutil
import subprocess
import sys
from pathlib import Path

repo = Path(__file__).resolve().parent.parent
out = Path(sys.argv[1]).resolve() / 'firmwares'
shutil.rmtree(out, ignore_errors=True)
import os
prebuilt = os.environ.get('OPENHOUSE_FIRMWARES')
if prebuilt:
    if not (Path(prebuilt) / 'dongle').is_dir():
        sys.exit(f'stage-firmwares: {prebuilt} isn\'t a firmwares folder')
    shutil.copytree(prebuilt, out)
    print(f'Staged firmware from {prebuilt} in {out}')
    sys.exit(0)
for sub in ('basestation', 'dongle', 'radio'):
    (out / sub).mkdir(parents=True)

def make(path, *args):
    subprocess.run(['make', '-s', '-C', str(repo / path), *args], check=True)

make('basestation_firmware')
image = repo / 'basestation_firmware/build/firmware.bin'
data = image.read_bytes()
if sum(int.from_bytes(data[i:i + 4], 'little') for i in range(0, 32, 4)) % 2**32:
    sys.exit('lhbs: bad vector checksum')
# The commit, from git, or (no checkout: the Nix flake) OPENHOUSE_BUILD_COMMIT.
import os
commit = os.environ.get('OPENHOUSE_BUILD_COMMIT', '').removesuffix('-dirty')
if not commit:
    try:
        commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', '--short', 'HEAD'], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        commit = 'local'   # no checkout and no commit given (as the CMake version stamp says)
shutil.copy2(image, out / 'basestation' / f'lhbs-{datetime.date.today():%Y%m%d}-{commit}.bin')

make('doorman/dongle-fw')
# Every board's zip (a board file removed since can leave its old zip in build/zips: only current ones)
import json
for board in sorted((repo / 'doorman/boards').glob('*.json')):
    if json.loads(board.read_text()).get('kind') == 'check':
        continue
    shutil.copy2(repo / f'doorman/dongle-fw/build/zips/{board.stem}.zip', out / 'dongle' / f'{board.stem}.zip')

make('doorman/radio-fw')
make('doorman/radio-fw', 'TARGET=idx')
shutil.copy2(repo / 'doorman/radio-fw/build-idx/radio_idx.fw', out / 'radio/doorman-radio-wm3.fw')
shutil.copy2(repo / 'doorman/radio-fw/build/radio.bin', out / 'radio/doorman-radio-neo.bin')
print(f'Staged firmware in {out}')
