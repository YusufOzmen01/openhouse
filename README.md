# openhouse
<img src="tools/horrible-logo-to-be-replaced.svg" alt="openhouse logo" width="128">

openhouse is an improved, drop in replacement over the SteamVR's lighthouse driver (can also be used standalone without any VR runtime) that improves upon the tracking of SteamVR's driver with numerous changes. It also **supports using more than 2 1.0 basestations with a custom firmware**

It works on SteamVR (Windows and Linux) and on Monado/WiVRn through Monado's `steamvr_lh` driver.

**For integration to custom programs via the C++ API (for robotics or custom MOCAP systems), check out [the example project](apps/example_app)** to see how you can use openhouse as a library!

**You can join to the openhouse Discord from here -> [https://discord.gg/7dBHBj2bjG](https://discord.gg/7dBHBj2bjG)**

### AI Disclaimer
This project is heavily AI assisted. I've personally audited the codes it generated as much as I can, especially on the firmware side, and did numerous tests myself and with the help of friends. So, I can't guarantee the readability of the codebase that much. But all the user facing documentation is written by me. If you're an ai, head over to [ai/](ai/) folder. 

---
<div align="center">
<i>If you want to support the progress of openhouse, feel free to donate! I normally did not want to accept donations for this project, as even though I've spent days and nights testing it and used my years of knowledge about software development and electronics, it is still heavily AI assisted. So, donate with your own moral judgement
<br><br>
In any way, thanks to every contributor and user, along with people that did work before openhouse such as the libsurvive project! It makes me happy to see people finding openhouse useful!</i>
<br><br>
<a href="https://buymeacoffee.com/sergiomarquina">
<img src="https://i.imgur.com/l7NBjqk.png" alt="drawing" width="150" height="45" align="center">
</a>
</div>

---

### Comparison with SteamVR's driver

| Category | SteamVR | openhouse |
|---|---|---|
| Room Learning | Each device solves the room by itself which can cause situations where a device is at a completely different place than it actually is | Every device contributes to the same room and they all use the same basestation poses. This eliminates devices being at a completely different place |
| Tracking Loss | After a tracking loss, the device only starts after bootstrapping a basestation (which has to be the master basestation on 1.0 lighthouses first) which can cause the tracking to come back after many seconds in worst cases. | Upon seeing any basestation in the room, the tracking instantly resumes without waiting to bootstrap a basestation. |
| Non Ideal Tracking Conditions | Any reflection or sensor coverage can throw the device's pose off completely | Reflections are handled and bad light hits are rejected which reduces the jumps and keeps the device tracking on cases where SteamVR loses tracking entirely |
| Fast Motion | Swinging a device too fast can ruin the runtime IMU calibration or reach to the limits of the IMU, causing the device to completely lose tracking. | The filter dynamically disables parts that are slow to improve tracking on very fast movements, which reduces the amount of tracking loss immensely |
| Jitter and Stutters | Tracking corrections cause jitter and sudden movements on the devices | Output is predicted and smoothed, late wireless light is used instead of dropped, and still devices are held perfectly still |
| Lighthouse 1.0 Support | Up to 2 basestations | Up to 4 basestations with the lhbs firmware (included) |
| Lighthouse 2.0 Support | Up to 4 basestations used actively | Still 4 actively being used, but the switch that happens when one basestation is not visible is immediate, unlike SteamVR |
| Mixed 1.0 and 2.0 Basestations | Not supported. A room has to be all 1.0 or all 2.0 basestations | Experimental: 1.0 and 2.0 basestations can be used together in one room (see [Mixed basestations](#mixed-basestations-experimental)) |

Everything else that you'd expect still works. Headsets (tested on HTC Vive and Bigscreen Beyond, other headsets probably also work), controllers (Vive and Index), trackers (Vive, Tundra, modded vive controllers that appears as trackers etc.) along with SteamVR's pairing menu, power features such as powering the device off when SteamVR closes and so on.

## Installation
Grab the latest release for your platform. **Close SteamVR (and Monado/WiVRn) before installing!**

The installer replaces SteamVR's `driver_lighthouse` and keeps the original one next to it as `driver_lighthouse.so.stock` / `driver_lighthouse.dll.stock`. openhouse needs that file to show the headset's display. The driver doesn't contain anything from the stock driver. It only loads the driver when a lighthouse headset is connected.

SteamVR updates could overwrite the driver. The installer also adds a small update check to SteamVR that starts with it, and if a SteamVR update replaced openhouse, it shows a dialog telling you to run the installer again. SteamVR only starts it once your headset is up, so if you start SteamVR without a headset you won't get the dialog. I'd advise you to run the installer every time SteamVR updates just in case. On Linux, the installer also asks if you want the same check for Monado/WiVRn. If you say yes, it installs a systemd user service that checks every time Monado or WiVRn starts. The uninstaller removes all of it.

To see which version you have, open `openhouse-overlay` (About) or look for `openhouse` in SteamVR's `vrserver.txt`.

### Windows
Extract the zip somewhere and double click `openhouse-installer.exe`, then pick 1 to install. To go back to SteamVR's driver, run it again and pick 2. If Steam or SteamVR is open, it asks to close them first.

If your SteamVR is in another Steam library, run it from a command prompt like `openhouse-installer.exe --install --steamvr "D:\SteamLibrary\steamapps\common\SteamVR"`.

### Linux
Register the openhouse driver to replace the lighthouse driver with `packaging/steamvr-external-driver.sh register`, this will disable lighthouse and set openhouse as a loaded external_driver. Unregister and reenable lighthouse with `packaging/steamvr-external-driver.sh unregister`. Depends on `jq` package being installed on system to parse the steam configs.

### 1.0 basestation firmware (lhbs)
#### **THIS IS ONLY NEEDED IF YOU OWN 1.0 BASESTATIONS AND WANT TO USE MORE THAN TWO OF THEM AT THE SAME TIME. DO NOT FLASH THIS ON 2.0 BASESTATIONS!!!!!!!** 
lhbs keeps everything the original firmware does (A/b/c channels, sync cable), and adds a multi-station mode for up to 4 basestations.

Get `openhouse-basestation-firmware.zip`, extract it, and:
- **Windows:** double click `Install-Firmware.bat`
- **Linux:** run `./install-firmware.sh`

The script walks you through it. In short, unplug the basestation's power, plug the micro USB to your PC, hold the button on the back and then connect the power cable. A drive called `CRP DISABLD` shows up. Type the serial from the sticker on the basestation (it's used for naming the backup) and confirm. Your current firmware gets backed up into the `backups` folder before anything gets written, and after flashing it asks you to replug it the same way so it can verify the firmware.

Do one basestation at a time. **Do not copy the firmware file onto the drive yourself!!!** as that can end up writing it to the wrong place, causing the firmware to get corrupted.

To go back to your old firmware: `Install-Firmware.bat -Restore backups\LHB-XXXXXXXX-....bin` on Windows, `./install-firmware.sh --restore backups/LHB-XXXXXXXX-....bin` on Linux. A 1.0 basestation can never get bricked so you can restore the stock firmware if something goes bad.

#### Setting the channels
Press the button on the back of the basestation to change its channel. With lhbs, it goes `A` -> `b` -> `c` -> `1` -> `2` -> `3` -> `4` and back to `A`. The channel is saved, so you only need to do this once

- `A`, `b` and `c` are the normal channels and work exactly like the stock firmware (for 1 or 2 basestations).
- `1` to `4` are the multi-station channels, for 2 to 4 basestations:
  - Set **one** basestation to `1`. This is the master basestation
  - Give every other basestation a different number (`2`, `3`, `4`). Two basestations on the same number won't work.
  - Every basestation has to see the master's light, since that's how they stay in sync (sync cables do not work).
  - You don't have to use every number. For 3 basestations, `1` `2` `3` is fine, and they'll share the sweeps between the 3 of them automatically.
- Don't mix the normal and the multi-station channels in one room. Either all of them are on `A`/`b`/`c`, or all of them are on `1`-`4`.

openhouse detects the multi-station mode by itself, nothing needs to be set in the driver. The stock SteamVR driver can't use the multi-station channels, so switch back to `A`/`b`/`c` if you go back to it. If tracking doesn't start properly, power cycle the basestations once by unplugging the power adapter and replugging it, which fixes stale mode issues sometimes

## Usage

### SteamVR
After installing, just start SteamVR. That's it. The first time, the room takes a bit to learn. Move your devices around so they see more than one basestation at once. It gets saved, so the next time it's instant.

### Monado / WiVRn
Install it into SteamVR's folder like above, and start Monado with its `steamvr_lh` driver (`STEAMVR_LH_ENABLE=1`).

### openhouse-overlay
`openhouse-overlay` is the main overlay/manager for openhouse. The installer adds it to SteamVR, so it starts with SteamVR and you'll find an openhouse button in the SteamVR dashboard. Under Monado/WiVRn (or without SteamVR running) you can open it as a normal window, it's in the `tools` folder on Linux and the `apps` folder on Windows. It has:
- **Overview:** the room status, your devices (tracking state, battery, how many basestations see them) and your basestations (placed, learning, locked etc)
- **Devices:** details for each device, along with the ability to power them off or vibrate them
- **Base stations:** where each basestation is, how well it's placed and which devices see it
- **Dongles:** lets you see each dongle, which device they are paired and put individual dongles into pairing mode
- **Room map:** a 3D view of the room and how far the room learning is. Under Monado/WiVRn you can also set the floor level from here (put your controllers on the floor and wait 7 seconds), which is not needed for SteamVR as SteamVR has its own Room Setup
- **Settings:** smoothing, acceleration prediction, hiding lost devices, the mixed 1.0/2.0 options and **Use SteamVR's rooms** (see [Settings](#settings)). They're saved to openhouse's `settings.json` and apply while you play
- **Debug:** detailed debug information about tracking
- **Record session:** you can record a session through here to send it for bug reports
- **Reset room:** lets you reset the room map (you need to do Room Setup again if you've done it)
- **Copy vrserver logs:** copies SteamVR's `vrserver.txt` to your clipboard, so you can paste it into a bug report or a message

### openhouse-compare
`openhouse-compare` (next to `openhouse-overlay`) runs openhouse and SteamVR's own lighthouse driver at the same time on the same devices, so you can see the difference yourself. Close SteamVR first, it runs both drivers by itself. Pick a device on the left, openhouse is shown in blue and SteamVR in red, with how much each one tracked, coasted and lost it.

### Settings
Settings are in `~/.config/lh/` on Linux and `%APPDATA%\lh\` on Windows.
- `roommap.json` is the learned room. Delete it if you want to learn the room from scratch. You can delete it while openhouse is running too, it notices and starts learning the room again right away (same as **Reset room** in openhouse-overlay).
- `hostframe.json` is where your play area is anchored. Delete it if you want to re-anchor.
- `settings.json` has openhouse's settings. openhouse-overlay's Settings page changes it for you, but you can also edit it by hand (smoothing, prediction, hiding lost devices and the mixed options apply within a second, the rest after a restart). You can also use it to disable/enable devices, set manual device roles etc.
```json
{
  "roles": { "LHR-180D3882": "generic_tracker" },
  "controllers": { "left": "LHR-8F9BE6F2", "right": "LHR-13DA81D1" },
  "disabledDevices": ["LHR-0AC3E7C8"],
  "positionSmoothingMs": 60,
  "rotationSmoothingMs": 60,
  "predictWithAcceleration": true,
  "hideLostDevices": false,
  "useSteamVRRooms": true,
  "mixedLight": { "enabled": true, "controllers": true, "trackers": true, "scout": true, "rotorReference": true, "preferLh2": false }
}
```
`roles` changes a device's role (`left_hand`, `right_hand` or `generic_tracker`, for example a Vive controller on your hip as a tracker), `controllers` picks which controller is left and right, and `disabledDevices` makes openhouse ignore a device completely.
`hideLostDevices` makes controllers and trackers disappear while they've lost tracking, kinda like the stock driver, instead of staying where they were last seen (off by default). The headset is always kept where it was. It's **Hide lost devices** in openhouse-overlay.
`positionSmoothingMs` and `rotationSmoothingMs` are how long a tracking correction takes to show up, 60 ms by default. Lower means less delay, but small corrections can show up as tiny jumps. 0 turns it off.
`predictWithAcceleration` helps SteamVR predict where your controllers will be during fast motion, like in Beat Saber. On by default.
`useSteamVRRooms` (**Use SteamVR's rooms** in openhouse-overlay, SteamVR only, on by default) keeps openhouse in step with SteamVR's own room files, so Room Setup isn't asked for on every start. If you already did Room Setup with SteamVR's driver for the same basestations, openhouse uses that room (its play area and standing height). Otherwise openhouse adds its room to SteamVR's `lighthousedb.json` (a backup of the original is kept as `lighthousedb.json.openhouse-backup`), so Room Setup is done once and remembered, and other apps that read SteamVR's room see where the basestations are. SteamVR's own rooms are never changed. Turn it off (and restart SteamVR) to keep openhouse's room completely separate from SteamVR's, as in 1.0.2.
`mixedLight` (experimental, off by default) uses HTC Vive 1.0 and 2.0 base stations together in one room; `"mixedLight": true` is enough to turn it on. The same options are under **Mixed basestations** in openhouse-overlay.
- `preinit/` is optional. Device configs in it (`<serial>.json`) are created when openhouse starts and show up as connected but not tracking until you turn them on (after that they connect and disconnect normally), even under Monado, which only takes devices at startup. Save the configs of your connected devices with `lhtool configs ~/.config/lh/preinit` (close SteamVR/Monado first, turn everything on). If a device's own config differs from its file, openhouse uses the device's and updates the file.

If you used the openhouse page in SteamVR's settings in an older version, your settings there are moved into `settings.json` the first time the new version starts, and the installer removes that page.

### Mixed basestations (experimental)
openhouse can use HTC Vive 1.0 and 2.0 basestations together in one room, which SteamVR can't. Turn it on with **Mixed basestations > Enabled** in openhouse-overlay's Settings, or `"mixedLight": true` in `settings.json`. (you can also enable **Rotor reference** to improve tracking even further) Things to know:
- **Devices that do not work with 2.0 lighthouses can't be used** which includes the original HTC Vive headset, Vive wands and the 2017 Vive trackers (ones with gray power button instead of blue)
- **The headset will stay on 2.0 mode at all times.** It times the 2.0 basestations for the other devices (the **Headset times the 2.0 stations** toggle), so controllers and trackers have to switch less often. Switching that toggle off will cause the headset to blank the view every time a switch happens, so keep that in mind
- **The headset also measures the 2.0 basestations' timing for the other devices** (the **Rotor reference** toggle, on by default). A 2.0 basestation's rotor doesn't spin perfectly evenly, and in mixed mode that shows up as a small wobble on controllers and trackers. The headset sees the real timing, so the others take that wobble off while the headset can see the same basestation
- **1.0 as fallback only** is a test, off by default: devices stay on 2.0 light while they can, which is steadier, and only read the 1.0 basestations when the 2.0 ones don't cover them
- **The first few minutes might be a bit problematic** as openhouse has to work out whether you're in a mixed room at all, so tracking can be shakier during the first session.
- **It takes longer to settle.** In a mixed room, learning the room's layout takes a bit more time than with one kind of basestation. That is normal, and give it some time while the room is learned
- If you encounter a problem with the room map or tracking, **click Reset room in openhouse-overlay, or remove the roommap.json file in ~/.config/lh in Linux**
- Controllers and trackers can be disabled separately for this mode, so you can only enable it on the type of device that you want to use with mixed basestation mode

### Reporting a bug
1. Click **Record session** in openhouse-overlay.
2. Play until the problem happens and keep using it for a minute after it.
3. Click **Stop recording**. The overlay shows where the bug report was saved.

That saves a zip into `Documents/openhouse reports` which contains the recording (the recordings themselves stay in `~/.local/share/openhouse/recordings` on Linux and `%LOCALAPPDATA%\openhouse\recordings` on Windows), your room map, openhouse's settings files (`settings.json`, `hostframe.json`, `preinit/`) and SteamVR's logs. Link the zip that is generated to me on either GitHub issues or through Discord/Twitter

If the problem happens right when SteamVR starts, set `LH_CAPTURE=/some/folder` (or add `"capture": "C:\\lh-capture\\run1"` under `"driver_lighthouse"` in `steamvr.vrsettings`) to record the whole session from the start. That recording isn't put in a zip, so send me that folder along with SteamVR's `vrserver.txt`. Don't use **Record session** at the same time, as it takes over the recording.

You can also record without SteamVR with `lhcapture` (close SteamVR first). While it's recording, you can type a note and press enter to mark the time, like `left foot jumping`.

### Tools and the library
- `lhcapture` records your devices' raw data.
- `lhtool session FILE.lhraw` replays a recording through the whole tracking and prints what happened (`--trace OUT.jsonl` for every pose, `--from`/`--to` for a part of it). `lhtool stats` shows per device data rates, `lhtool haptics SERIAL` tests a controller's vibration, `lhtool configs DIR` saves every connected device's config to `DIR`.
- `lhviz` is a 3D viewer that runs its own tracking without SteamVR, mostly for development or testing

All of the tracking is in a C++ library (`liblh`), separate from the SteamVR driver, so you can use it in your own app without SteamVR: `lh::Session` (`include/lh/session.hpp`) opens the devices, tracks them and gives you their poses, inputs and the basestations. The tools above are built on it, so you can look at them to see how the library can be used.

### Building
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```
Needs Eigen, Ceres, nlohmann-json and GTest (and glfw for the viewers). The tests use short recordings from `tests/data/captures`.

For the release packages, `scripts/build-linux.sh` builds and packages the Linux one, `scripts/build-windows.sh` cross builds and packages the Windows one with MinGW, and `scripts/build-firmware.sh` builds, tests and packages the lhbs firmware (needs `arm-none-eabi-gcc`). Both build their own Ceres through vcpkg, since the distro Ceres usually comes with SuiteSparse, which has GPL parts that can't be shipped with openhouse.

### Environment variables
You don't need any of these normally. On Linux, put them in SteamVR's launch options like `LH_LOG_LEVEL=DEBUG %command%`. On Windows, set them as system environment variables and restart Steam.

| Variable | What it does |
|---|---|
| `LH_LOG_LEVEL=DEBUG` | Shows DEBUG logs in SteamVR's logs (`INFO` by default, also `WARN` and `ERR`) |
| `LH_TELEMETRY_PORT=N` | The port openhouse-overlay connects to (36840 by default, `0` turns it off, and the overlay with it) |
| `LH_CAPTURE=DIR` | Records the whole session from the start into a folder (see Reporting a bug) |
| `LH_NO_POWER_OFF_ON_EXIT=1` | Don't turn off devices when SteamVR exits, overrides SteamVR's setting |
| `LH_STOCK_HMD=1` / `LH_NO_STOCK_HMD=1` | Always/never load SteamVR's display driver for the headset |
| `LH_STOCK_DRIVER=PATH` | Where SteamVR's original driver is, if it's not next to ours |
| `LH_STOCK_ALL_DEVICES=1` | Let SteamVR's driver see every device, not only the headset. Not recommended as it causes issues with tracking |
| `LH_ROOM_MAP=FILE` | Use another room file |
| `LH_STEAM_DB=auto\|-` | Import the basestation positions from SteamVR's own room (or `=FILE`); `-` never imports. Under SteamVR it's on with **Use SteamVR's rooms** |
| `LH_HOST_FRAME=steamvr\|openhouse` | `steamvr`: always use SteamVR's own play space (`LH_REFERENCE_DB=FILE` for the room file it uses). `openhouse`: always openhouse's, and SteamVR's room files are left alone (as **Use SteamVR's rooms** off) |
| `LH_MULTISTATION=on\|off` | Force the lhbs multi-station mode on or off for 1.0 basestations |
| `LH_PREFER_STEAM_CONFIG=1` | Use SteamVR's cached device configs instead of reading them from the devices |
| `LH_STEAM_ROOT`, `LH_STEAM_CONFIG_DIR`, `STEAMVR_PATH` | Where Steam and SteamVR are, if they can't be found |
| `LH_HAND_POSES`, `LH_HAND_ANIMS` | Where the Index hand animation files are, if they can't be found |
| `LH_REPLAY=FILE.lhraw` | Run the driver on a recording instead of your devices |
| `LH_STOCK_FILTER=1` | Turns off openhouse's tracking improvements and uses the stock driver's tracking |
| `LH_EVALUATE_ROOM=1` | Freezes the room (development) |

## Credits

Huge thanks to the [libsurvive](https://github.com/collabora/libsurvive) project. Their work on the watchman protocol, device and light decoding, controller inputs and the 2.0 basestations was used as a reference a lot while making openhouse.

## Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org).

The Windows release (`openhouse-windows-x64.zip`) is built from this repository's source by [GitHub Actions](.github/workflows/build.yml) and signed there

Team roles:
- Committers and reviewers: [members](https://github.com/YusufOzmen01/openhouse/graphs/contributors)
- Approvers: [YusufOzmen01](https://github.com/YusufOzmen01).

## Privacy
See [PRIVACY.md](PRIVACY.md)

## License

MIT. See [LICENSE](LICENSE).

## Disclaimer

This project is not affiliated with, endorsed by, or sponsored by Valve Corporation or HTC Corporation. Valve, SteamVR and Lighthouse are trademarks of Valve Corporation. HTC and VIVE are trademarks of HTC Corporation. This repository does  not contain any Valve of HTC property. All of the work here is reverse engineering with improvements over the said reverse engineered code.
