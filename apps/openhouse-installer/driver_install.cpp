#include "driver_install.hpp"
#include "shortcuts.hpp"
#include "lh/udev_rules.h"
#include "lh/version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openhouse::installer {
namespace {
#ifdef _WIN32
constexpr const char* kBleLibrary = "lighthouse.dll";
#else
constexpr const char* kBleLibrary = "liblighthouse.so";
#endif
using ordered_json = nlohmann::ordered_json;

constexpr const char* kMarker = "liblh session started";       // in every openhouse driver
constexpr const char* kBuildTimeMarker = "openhouse-build-time:";   // when it was built, Unix seconds
constexpr const char* kVersionMarker = "openhouse-version:";     // since 1.0.4: "openhouse-version:X.Y.Z;"
constexpr const char* kCheckKey = "openhouse.updatechecker";
constexpr const char* kOverlayKey = "openhouse.overlay";

#ifdef _WIN32
constexpr const char* kBin = "win64";
constexpr const char* kDriverFile = "driver_lighthouse.dll";
constexpr const char* kExe = ".exe";
#else
constexpr const char* kBin = "linux64";
constexpr const char* kDriverFile = "driver_lighthouse.so";
constexpr const char* kExe = "";
#endif

fs::path VrPathFile() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH * 2];
  const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, DWORD(std::size(buffer)));
  return n ? fs::path(buffer) / "openvr" / "openvrpaths.vrpath" : fs::path();
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  return (xdg && *xdg ? Path(xdg) : HomeDirectory() / ".config") / "openvr" / "openvrpaths.vrpath";
#endif
}

nlohmann::json ReadJson(const fs::path& p) {
  std::string text;
  if (!ReadFile(p, &text)) return nlohmann::json::object();
  nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
  return j.is_discarded() ? nlohmann::json::object() : j;
}

struct Paths {
  fs::path steamvr, directory, target, backup, schema, defaults, data, config;
};

fs::path SteamConfig(const fs::path& steamvr) {
  const nlohmann::json paths = ReadJson(VrPathFile());
  const auto c = paths.find("config");
  if (c != paths.end() && c->is_array() && !c->empty() && (*c)[0].is_string()) return Path((*c)[0].get<std::string>());
  return steamvr.parent_path().parent_path().parent_path() / "config";  // .../Steam/config
}

Paths MakePaths(const fs::path& steamvr) {
  Paths p;
  p.steamvr = steamvr;
  p.directory = steamvr / "drivers" / "lighthouse" / "bin" / kBin;
  p.target = p.directory / kDriverFile;
  p.backup = p.target;
  p.backup += ".stock";
  p.schema = steamvr / "drivers" / "lighthouse" / "resources" / "settings" / "settingsschema.vrsettings";
  p.defaults = p.schema.parent_path() / "default.vrsettings";
  p.data = DataDirectory();
  p.config = SteamConfig(steamvr);
  return p;
}

std::string Contents(const fs::path& p) {
  std::string s;
  // An unreadable driver read as "" would look like SteamVR's own, and uninstall would delete
  // the stock backup: stop instead.
  if (!ReadFile(p, &s)) throw std::runtime_error("Couldn't read " + Utf8(p) + ". Is SteamVR still running?");
  return s;
}

// An app SteamVR starts with every session: its manifest in Steam's appconfig.json, and
// auto-launch in vrappconfig/<key>.vrappconfig.
void SetRegistration(const Paths& p, const fs::path& manifest, const char* key, bool on, const std::function<void(const std::string&)>& log) {
  const fs::path appconfig = p.config / "appconfig.json";
  ordered_json data = ordered_json::object();
  std::error_code ec;
  if (fs::exists(appconfig, ec)) {
    std::string text;
    // Steam's list of every registered VR app: one we can't read is left alone, as rewriting
    // it from an empty object dropped all the others.
    if (!ReadFile(appconfig, &text)) { log("Couldn't read " + Utf8(appconfig) + "; left as it is."); return; }
    if (!text.empty()) {
      data = ordered_json::parse(text, nullptr, false);
      if (!data.is_object()) { log(Utf8(appconfig) + " isn't what Steam writes; left as it is."); return; }
    }
  }
  ordered_json kept = ordered_json::array();
  if (data.contains("manifest_paths") && data["manifest_paths"].is_array())
    for (const auto& m : data["manifest_paths"])
      // Any earlier registration of this app goes, wherever its manifest was (an older install's
      // data folder, another user name, a moved Steam), so only the one written now stays.
      if (m.is_string() && Path(m.get<std::string>()).filename() != manifest.filename()) kept.push_back(m);
  if (on) kept.push_back(Utf8(manifest));
  data["manifest_paths"] = kept;
  if (!WriteFileAtomic(appconfig, data.dump(3) + "\n")) throw std::runtime_error("Couldn't write " + Utf8(appconfig));
  const fs::path app = p.config / "vrappconfig" / (std::string(key) + ".vrappconfig");
  if (on) WriteFileAtomic(app, "{\n   \"autolaunch\" : true,\n   \"last_launch_time\" : \"0\"\n}\n");
  else RemoveFile(app);
}

void WriteManifest(const fs::path& path, const char* key, const fs::path& binary, const std::string& arguments,
                   const char* name, const char* description) {
  ordered_json app;
  app["app_key"] = key;
  app["launch_type"] = "binary";
#ifdef _WIN32
  app["binary_path_windows"] = Utf8(binary);
#else
  app["binary_path_linux"] = Utf8(binary);
#endif
  app["arguments"] = arguments;
  app["is_dashboard_overlay"] = true;
  app["strings"]["en_us"] = {{"name", name}, {"description", description}};
  ordered_json manifest;
  manifest["source"] = "openhouse";
  manifest["applications"] = ordered_json::array({app});
  if (!WriteFileAtomic(path, manifest.dump(3) + "\n")) throw std::runtime_error("Couldn't write " + Utf8(path));
}

// The page older versions added to SteamVR's settings (settings are openhouse-overlay's now):
// its schema, and its line in SteamVR's default.vrsettings.
void RemoveOldSettingsPage(const Paths& p) {
  std::error_code ec;
  std::string text;
  fs::path stock = p.schema;
  stock += ".stock";
  if (fs::exists(p.schema, ec) && ReadFile(p.schema, &text) && text.find("driver_openhouse") != std::string::npos) RemoveFile(p.schema);
  if (fs::exists(stock, ec)) {
    // Still there: a SteamVR update replaced ours with a newer one, so the backup is older.
    if (fs::exists(p.schema, ec)) RemoveFile(stock);
    else fs::rename(stock, p.schema, ec);
  }
  if (!ReadFile(p.defaults, &text)) return;
  const size_t at = text.find("\n\"driver_openhouse\": {");
  if (at == std::string::npos) return;
  size_t end = text.find('\n', at + 1);
  if (end == std::string::npos) end = text.size();
  else if (end > 0 && text[end - 1] == '\r') --end;  // keep the next line's break as it was
  text.erase(at, end - at);
  WriteFileAtomic(p.defaults, text);
}

#ifndef _WIN32
fs::path MonadoUnit() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  return (xdg && *xdg ? Path(xdg) : HomeDirectory() / ".config") / "systemd" / "user" / "openhouse-update-checker.service";
}

fs::path EnvisionConfig() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  return (xdg && *xdg ? Path(xdg) : HomeDirectory() / ".config") / "envision" / "envision.json";
}

// Envision runs only the plugins ticked in its Preferences: their .desktop paths (canonical, as it
// compares them) in its config's xr_plugins. Envision writes its whole config back from memory, so
// it's only edited while Envision isn't running. False when it is, or there's no config to edit.
// Whether Envision has that entry ticked as a plugin.
bool EnvisionPluginTicked(const fs::path& desktop) {
  std::error_code ec;
  std::ifstream in(EnvisionConfig());
  const auto j = nlohmann::json::parse(in, nullptr, false);
  if (!j.is_object() || !j.contains("xr_plugins") || !j["xr_plugins"].is_array()) return false;
  const std::string path = Utf8(fs::weakly_canonical(desktop, ec));
  for (const auto& e : j["xr_plugins"]) if (e.is_string() && e.get<std::string>() == path) return true;
  return false;
}

bool EnableEnvisionPlugin(const fs::path& desktop, bool on) {
  const fs::path config = EnvisionConfig();
  std::error_code ec;
  if (!fs::exists(config, ec) || RunProcess({"pgrep", "-x", "envision"}, nullptr) == 0) return false;
  nlohmann::json j;
  try {
    std::ifstream in(config);
    j = nlohmann::json::parse(in);
  } catch (const std::exception&) {
    return false;
  }
  if (!j.is_object()) return false;
  const std::string path = Utf8(fs::weakly_canonical(desktop, ec));
  nlohmann::json plugins = nlohmann::json::array();
  if (j.contains("xr_plugins") && j["xr_plugins"].is_array())
    for (const auto& e : j["xr_plugins"])
      if (!(e.is_string() && e.get<std::string>() == path)) plugins.push_back(e);
  if (on) plugins.push_back(path);
  if (j.contains("xr_plugins") && j["xr_plugins"] == plugins) return true;
  j["xr_plugins"] = plugins;
  return WriteFileAtomic(config, j.dump(2) + "\n");
}

// For Monado / WiVRn (they don't start SteamVR apps): a systemd user service that runs the
// update check (openhouse-overlay --check) whenever monado-service or wivrn-server starts.
void InstallMonadoCheck(const Paths& p) {
  const fs::path watch = p.data / "openhouse-monado-watch";
  const std::string check = Utf8(p.data / "openhouse-overlay");
  WriteFileAtomic(watch,
                  "#!/usr/bin/env bash\n"
                  "# Runs openhouse's update check once each time Monado or WiVRn starts.\n"
                  "running=0\n"
                  "while sleep 3; do\n"
                  "  if pgrep -x monado-service >/dev/null || pgrep -x wivrn-server >/dev/null; then\n"
                  "    [[ $running == 0 ]] && \"" + check + "\" --check \"" + Utf8(p.directory) + "\"\n"
                  "    running=1\n"
                  "  else\n"
                  "    running=0\n"
                  "  fi\n"
                  "done\n");
  std::error_code ec;
  fs::permissions(watch, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read, ec);
  WriteFileAtomic(MonadoUnit(),
                  "[Unit]\nDescription=openhouse: warn when a SteamVR update replaced the openhouse driver (Monado / WiVRn)\n\n"
                  "[Service]\nExecStart=" + Utf8(watch) + "\nRestart=on-failure\n\n[Install]\nWantedBy=default.target\n");
  RunProcess({"systemctl", "--user", "daemon-reload"}, nullptr);
  RunProcess({"systemctl", "--user", "enable", "openhouse-update-checker.service"}, nullptr);
  RunProcess({"systemctl", "--user", "restart", "openhouse-update-checker.service"}, nullptr);
}

void RemoveMonadoCheck() {
  std::error_code ec;
  if (!fs::exists(MonadoUnit(), ec)) return;
  RunProcess({"systemctl", "--user", "disable", "--now", "openhouse-update-checker.service"}, nullptr);
  RemoveFile(MonadoUnit());
  RunProcess({"systemctl", "--user", "daemon-reload"}, nullptr);
}
#endif

// 1.0.3 and older ran the update check as openhouse-check (key openhouse.check): taken out so
// only the renamed one starts.
void RemoveOldUpdateCheck(const Paths& p, const std::function<void(const std::string&)>& log) {
  std::error_code ec;
  const fs::path manifest = p.data / "openhouse-check.vrmanifest";
  if (fs::exists(manifest, ec)) SetRegistration(p, manifest, "openhouse.check", false, log);
  for (const char* name : {"openhouse-check", "openhouse-check.exe", "openhouse-check.vrmanifest"}) RemoveFile(p.data / name);
  // 1.0.4 moved the check into openhouse-overlay: the separate update-checker goes.
  const fs::path checker = p.data / "update-checker.vrmanifest";
  if (fs::exists(checker, ec)) SetRegistration(p, checker, kCheckKey, false, log);
  for (const char* name : {"update-checker", "update-checker.exe", "update-checker.vrmanifest"}) RemoveFile(p.data / name);
#ifndef _WIN32
  const fs::path unit = MonadoUnit().parent_path() / "openhouse-check.service";
  if (fs::exists(unit, ec)) {
    RunProcess({"systemctl", "--user", "disable", "--now", "openhouse-check.service"}, nullptr);
    RemoveFile(unit);
    RunProcess({"systemctl", "--user", "daemon-reload"}, nullptr);
  }
#endif
}
// Old copies CopyOver moved aside because a program still had them loaded (Windows).
void RemoveMovedAside(const Paths& p) {
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(p.directory, ec))
    if (Utf8(e.path().filename()).find(".old-") != std::string::npos) RemoveFile(e.path());
}

// A step after the driver itself: its failure is logged, it doesn't undo or stop the rest.
void Try(const std::function<void(const std::string&)>& log, const char* what, const std::function<void()>& step) {
  try {
    step();
  } catch (const std::exception& e) {
    log(std::string(what) + ": " + e.what());
  }
}
}  // namespace

bool FlatpakSteam(const fs::path& root) {
#ifdef _WIN32
  (void)root;
  return false;
#else
  return Utf8(root).find("/.var/app/com.valvesoftware.Steam/") != std::string::npos;
#endif
}

bool IsSteamVRFolder(const fs::path& root) {
  std::error_code ec;
  return !root.empty() && fs::exists(root / "drivers" / "lighthouse" / "bin" / kBin / kDriverFile, ec);
}

SteamVR FindSteamVR() {
  SteamVR s;
  if (const std::string saved = LoadSetting("steamvr"); !saved.empty() && IsSteamVRFolder(Path(saved))) {
    s.root = Path(saved);
    s.saved = true;
    return s;
  }
  const nlohmann::json paths = ReadJson(VrPathFile());
  if (const auto r = paths.find("runtime"); r != paths.end() && r->is_array())
    for (const auto& e : *r)
      if (e.is_string() && IsSteamVRFolder(Path(e.get<std::string>()))) { s.root = Path(e.get<std::string>()); return s; }
  std::vector<fs::path> candidates;
#ifdef _WIN32
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &key) == ERROR_SUCCESS) {
    wchar_t value[MAX_PATH * 2]{};
    DWORD size = sizeof value;
    if (RegQueryValueExW(key, L"SteamPath", nullptr, nullptr, reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS)
      candidates.push_back(fs::path(value) / "steamapps" / "common" / "SteamVR");
    RegCloseKey(key);
  }
  candidates.push_back("C:/Program Files (x86)/Steam/steamapps/common/SteamVR");
#else
  const fs::path home = HomeDirectory();
  candidates = {home / ".local/share/Steam/steamapps/common/SteamVR", home / ".steam/steam/steamapps/common/SteamVR",
                home / ".steam/root/steamapps/common/SteamVR",
                home / ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/SteamVR"};  // last: not fully tested
#endif
  for (const auto& c : candidates)
    if (IsSteamVRFolder(c)) { s.root = c; return s; }
  return s;
}

void RememberSteamVR(const fs::path& root) { SaveSetting("steamvr", Utf8(root)); }

DriverStatus ReadStatus(const fs::path& steamvr) {
  DriverStatus st;
  const Paths p = MakePaths(steamvr);
  std::string text;
  std::error_code ec;
  st.backup = fs::exists(p.backup, ec);
  // The driver is read whole to find its markers, on every page the installer opens: a build with
  // debug info is 150 MB, a quarter of a second per click. Read again only when it changed.
  static std::mutex cacheMutex;
  static fs::path cachedPath;
  static std::uintmax_t cachedSize = 0;
  static fs::file_time_type cachedTime{};
  static DriverStatus cached;
  const std::uintmax_t size = fs::file_size(p.target, ec);
  const fs::file_time_type time = ec ? fs::file_time_type{} : fs::last_write_time(p.target, ec);
  if (!ec) {
    std::lock_guard lock(cacheMutex);
    if (cachedPath == p.target && cachedSize == size && cachedTime == time) {
      DriverStatus hit = cached;
      hit.backup = st.backup;
      return hit;
    }
  }
  st.driverFound = ReadFile(p.target, &text);
  if (!st.driverFound) return st;
  st.openhouse = text.find(kMarker) != std::string::npos;
  if (const size_t at = text.find(kVersionMarker); at != std::string::npos) {
    const size_t start = at + std::char_traits<char>::length(kVersionMarker);
    const size_t end = text.find(';', start);
    if (end != std::string::npos && end - start < 32) st.version = text.substr(start, end - start);
  }
  if (const size_t at = text.find(kBuildTimeMarker); at != std::string::npos) {
    const size_t start = at + std::char_traits<char>::length(kBuildTimeMarker);
    const size_t end = text.find(';', start);
    if (end != std::string::npos && end - start < 20) st.buildTime = std::atoll(text.substr(start, end - start).c_str());
  }
  if (!ec) {
    std::lock_guard lock(cacheMutex);
    cachedPath = p.target;
    cachedSize = size;
    cachedTime = time;
    cached = st;
  }
  return st;
}

bool PackageHasDriver() {
  std::error_code ec;
  return fs::exists(ExeDirectory() / kDriverFile, ec);
}

#ifndef _WIN32
namespace {
constexpr const char* kUdevPath = "/etc/udev/rules.d/60-openhouse.rules";
// kUdevRules: cmake/linux/60-openhouse.rules (generated/lh/udev_rules.h), what the Nix package installs too.

// One root shell command through pkexec (the desktop's password prompt).
bool AsRoot(const std::string& script, const std::vector<std::string>& args, std::string* error) {
  if (!HaveProgram("pkexec")) {
    *error = "pkexec not found. Run as root: " + script;
    return false;
  }
  std::vector<std::string> argv{"pkexec", "/bin/sh", "-c", script, "sh"};
  argv.insert(argv.end(), args.begin(), args.end());
  std::string last;
  const int rc = RunProcess(argv, [&](const std::string& l) { last = l; });
  if (rc == 126 || rc == 127) { *error = "the password prompt was cancelled"; return false; }
  if (rc != 0) { *error = last.empty() ? "it failed (" + std::to_string(rc) + ")" : last; return false; }
  return true;
}
}  // namespace
#endif

// NixOS: /etc is read-only and built from the configuration; the rule comes from openhouse's NixOS
// module (programs.openhouse.enable, flake.nix), never from here.
bool NixOS() {
  std::error_code ec;
  return fs::exists("/etc/NIXOS", ec);
}

bool DeviceAccessInstalled() {
#ifdef _WIN32
  return true;
#else
  std::string text;
  if (NixOS()) return ReadFile(kUdevPath, &text);   // the module's, whichever openhouse version it's from
  return ReadFile(kUdevPath, &text) && text == kUdevRules;
#endif
}

bool InstallDeviceAccess(std::string* error) {
#ifdef _WIN32
  (void)error;
  return true;
#else
  if (NixOS()) {
    *error = "On NixOS, add openhouse's NixOS module to your configuration (programs.openhouse.enable = true) and rebuild";
    return false;
  }
  const fs::path temp = DataDirectory() / "60-openhouse.rules";
  if (!WriteFileAtomic(temp, kUdevRules)) { *error = "couldn't write " + Utf8(temp); return false; }
  const bool ok = AsRoot(std::string("install -m 0644 \"$1\" ") + kUdevPath +
                             " && udevadm control --reload-rules && udevadm trigger --subsystem-match=hidraw --subsystem-match=usb --subsystem-match=tty",
                         {Utf8(temp)}, error);
  RemoveFile(temp);
  return ok;
#endif
}

bool RemoveDeviceAccess(std::string* error) {
#ifdef _WIN32
  (void)error;
  return true;
#else
  std::error_code ec;
  if (NixOS() || !fs::exists(kUdevPath, ec)) return true;   // NixOS: the module's, removed with it
  return AsRoot(std::string("rm -f ") + kUdevPath + " && udevadm control --reload-rules", {}, error);
#endif
}

MonadoCheck InstalledMonadoCheck() {
#ifdef _WIN32
  return MonadoCheck::None;
#else
  std::error_code ec;
  if (fs::exists(MonadoUnit(), ec)) return MonadoCheck::Systemd;
  if (fs::exists(ShortcutFile("update-check"), ec)) return MonadoCheck::Envision;
  return fs::is_directory(EnvisionConfig().parent_path(), ec) ? MonadoCheck::Envision : MonadoCheck::Systemd;
#endif
}

void InstallDriver(const fs::path& steamvr, const InstallOptions& options, const std::function<void(const std::string&)>& log) {
  const Paths p = MakePaths(steamvr);
  const fs::path here = ExeDirectory();
  const fs::path ours = here / kDriverFile;
  std::error_code ec;
  if (!fs::exists(ours, ec)) throw std::runtime_error("The package's driver is missing (" + Utf8(ours) + "). Extract the whole package and run the installer from there.");
  if (!fs::exists(p.target, ec)) throw std::runtime_error("SteamVR's lighthouse driver isn't at " + Utf8(p.target) + ". Check the SteamVR folder.");
  RemoveMovedAside(p);
  const bool isOurs = Contents(p.target).find(kMarker) != std::string::npos;
  if (!isOurs) {
    CopyOver(p.target, p.backup);
    log("Saved SteamVR's own driver as " + Utf8(p.backup.filename()));
  } else if (!fs::exists(p.backup, ec)) {
    throw std::runtime_error("openhouse is installed but SteamVR's own driver isn't kept next to it. Verify SteamVR's files in Steam (Properties > Installed Files), then install again.");
  }
#ifndef _WIN32
  // The driver's private libraries (lh-libs next to it).
  const fs::path libs = here / "lh-libs";
  if (fs::is_directory(libs, ec)) {
    // Replaced as a whole: a library an older version had and this one doesn't must not linger.
    fs::remove_all(p.directory / "lh-libs", ec);
    fs::create_directories(p.directory / "lh-libs", ec);
    for (const auto& e : fs::directory_iterator(libs, ec)) CopyOver(e.path(), p.directory / "lh-libs" / e.path().filename());
  }
#endif
  CopyOver(ours, p.target);
  if (Contents(p.target) != Contents(ours)) throw std::runtime_error("The installed driver doesn't match the package's. Close Steam and SteamVR, then try again.");
  log("Installed the openhouse driver");
#ifndef _WIN32
  if (!DeviceAccessInstalled()) {
    std::string error;
    if (InstallDeviceAccess(&error)) log("Set up device access (udev rule)");
    else log("Device access not set up: " + error);
  }
#endif
  RemoveOldSettingsPage(p);
  RemoveOldUpdateCheck(p, log);
  // The overlay; with --driver it also warns when a SteamVR update replaced openhouse, and offers
  // new releases (driver_check.hpp).
  const fs::path tools = here / "apps";
  fs::create_directories(p.data, ec);
  // What this install came from, kept for the overlay: Fix reinstalls from it (--repair) and
  // Update starts its installer (--update). A package of its own: installer, driver, its libraries.
  const fs::path saved = p.data / "package";
  if (fs::weakly_canonical(here, ec) != fs::weakly_canonical(saved, ec)) {
    fs::remove_all(saved, ec);
    fs::create_directories(saved / "apps", ec);
    for (const std::string name : {std::string("openhouse-installer") + kExe, std::string(kDriverFile), std::string("VERSION.txt")})
      if (fs::exists(here / name, ec)) CopyOver(here / name, saved / name);
    if (fs::exists(here / "lh-libs", ec)) {
      fs::create_directories(saved / "lh-libs", ec);
      for (const auto& e : fs::directory_iterator(here / "lh-libs", ec)) CopyOver(e.path(), saved / "lh-libs" / e.path().filename());
    }
    if (const fs::path o = tools / (std::string("openhouse-overlay") + kExe); fs::exists(o, ec)) CopyOver(o, saved / "apps" / o.filename());
    if (const fs::path b = tools / kBleLibrary; fs::exists(b, ec)) CopyOver(b, saved / "apps" / b.filename());
    // The dongle and radio firmware, for the overlay's firmware warning (it starts this copy with
    // --dongles): the dongles' zips (each also has its bootloader image) and the radio images.
    for (const char* sub : {"dongle", "radio"}) {
      const fs::path from = here / "firmwares" / sub;
      if (!fs::is_directory(from, ec)) continue;
      fs::create_directories(saved / "firmwares" / sub, ec);
      for (const auto& e : fs::directory_iterator(from, ec)) CopyOver(e.path(), saved / "firmwares" / sub / e.path().filename());
    }
    // The menu entries' apps and logos: a repair from this package puts them back too.
    if (const fs::path m = tools / (std::string("doorman-dongle-manager") + kExe); fs::exists(m, ec)) {
      try { CopyOver(m, saved / "apps" / m.filename()); } catch (const std::exception&) {}   // a copy is only for repairs
    }
    if (fs::is_directory(tools / "icons", ec)) {
      fs::create_directories(saved / "apps" / "icons", ec);
      for (const auto& e : fs::directory_iterator(tools / "icons", ec)) CopyOver(e.path(), saved / "apps" / "icons" / e.path().filename());
    }
#ifndef _WIN32
    fs::permissions(saved / "openhouse-installer", fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                    fs::perms::others_read | fs::perms::others_exec, ec);
#endif
    log("Kept the package for repairs");
  }
  RemoveFile(p.data / (std::string("openhouse-installer") + kExe));  // where 1.0.4 test builds kept it
  const fs::path overlay = tools / (std::string("openhouse-overlay") + kExe);
  if (fs::exists(overlay, ec)) {
    const fs::path installed = p.data / overlay.filename();
    CopyOver(overlay, installed);
    // ShayBox's Lighthouse library, where the overlay looks for it: its channel changes over Bluetooth.
    if (const fs::path b = tools / kBleLibrary; fs::exists(b, ec)) CopyOver(b, p.data / kBleLibrary);
    const fs::path manifest = p.data / "openhouse-overlay.vrmanifest";
    WriteManifest(manifest, kOverlayKey, installed, "--overlay --driver \"" + Utf8(p.directory) + "\"", "openhouse",
                  "openhouse's devices, base stations, room and settings in the SteamVR dashboard");
    SetRegistration(p, manifest, kOverlayKey, true, log);
    log("Added openhouse to SteamVR's dashboard");
  }
  // The apps people start themselves, from the menu.
  std::vector<Shortcut> shortcuts;
  // The package's PNG logos (Linux; Windows takes each program's own icon).
  auto icon = [&](const char* name) {
    const fs::path from = tools / "icons" / name, to = p.data / "icons" / name;
    if (!fs::exists(from, ec)) return fs::path();
    fs::create_directories(to.parent_path(), ec);
    try { CopyOver(from, to); } catch (const std::exception&) { return fs::path(); }
    return to;
  };
  if (fs::exists(p.data / overlay.filename(), ec)) {
    Shortcut s{"openhouse", "openhouse", "Devices, base stations, room and settings", p.data / overlay.filename(), icon("openhouse.png"), {}};
#ifndef _WIN32
    // Envision: the openhouse window as a plugin to opt into (it opens with each session), and the
    // update check as its own, hidden from the menu.
    if (options.monadoCheck == MonadoCheck::Envision) {
      s.xrPlugin = true;
      Shortcut check{"update-check", "openhouse update check", "Warns when a SteamVR update replaced the openhouse driver",
                     p.data / overlay.filename(), s.icon, {"--check", Utf8(p.directory)}};
      check.hidden = true;
      shortcuts.push_back(check);
    }
#endif
    shortcuts.push_back(s);
  }
  if (const fs::path m = tools / (std::string("doorman-dongle-manager") + kExe); fs::exists(m, ec)) {
    try {
      CopyOver(m, p.data / m.filename());
    } catch (const std::exception& e) {
      log(std::string("Dongle manager not updated (is it open?): ") + e.what());
    }
    if (fs::exists(p.data / m.filename(), ec)) shortcuts.push_back({"doorman-dongle-manager", "Doorman Dongle Manager", "Doorman Dongle pairing, channels and firmware", p.data / m.filename(), icon("dongle-manager.png"), {}});
  }
  // The installer this install came from (the saved package's): to reinstall, repair, uninstall or
  // flash firmware later without finding the extracted package again.
  if (const fs::path installer = saved / (std::string("openhouse-installer") + kExe); fs::exists(installer, ec))
    shortcuts.push_back({"installer", std::string("openhouse installer (v") + lh::kVersion + ")",
                         "Installs, repairs or removes openhouse, and flashes its firmware", installer, icon("openhouse.png"), {}});
  // Run from the Nix store (the flake's package): its own menu entries are already there (openhouse,
  // the dongle manager, the installer), so only the hidden Envision update check is written here.
  if (Utf8(here).rfind("/nix/store/", 0) == 0)
    shortcuts.erase(std::remove_if(shortcuts.begin(), shortcuts.end(), [](const Shortcut& s) { return !s.hidden; }), shortcuts.end());
#ifndef _WIN32
  // Before 1.0.5 the update check was the "openhouse" entry's own Envision plugin (--check). That
  // entry now opens the openhouse window: if Envision had it ticked for the check, it's unticked,
  // so the window doesn't start opening with every session (the check has its own entry now).
  // The entry was also renamed then (openhouse-openhouse.desktop -> openhouse.desktop, the window's app id,
  // so Wayland shows its icon): one Envision had ticked as the window's plugin is ticked again under its
  // new name once written.
  bool envisionHadWindow = false;
  {
    std::ifstream old(OldShortcutFile("openhouse"));
    const std::string text((std::istreambuf_iterator<char>(old)), std::istreambuf_iterator<char>());
    const auto plugin = text.find("X-XR-Plugin-Exec=");
    if (plugin != std::string::npos && text.find("--check", plugin) < text.find('\n', plugin))
      EnableEnvisionPlugin(OldShortcutFile("openhouse"), false);
    else envisionHadWindow = EnvisionPluginTicked(OldShortcutFile("openhouse"));
  }
#endif
  // SteamVR's firmware Update button on a device opens openhouse://update-dongles: the saved installer.
  if (!InstallUrlHandler(saved / (std::string("openhouse-installer") + kExe))) log("SteamVR's Update button won't open the updater");
  if (const std::string failed = InstallShortcuts(shortcuts); !failed.empty()) log("Menu entries not added: " + failed);
  else if (!shortcuts.empty()) log("Added the apps to the menu");
#ifndef _WIN32
  if (envisionHadWindow && EnableEnvisionPlugin(OldShortcutFile("openhouse"), false)) EnableEnvisionPlugin(ShortcutFile("openhouse"), true);
#endif
#ifndef _WIN32
  if (options.monadoCheck == MonadoCheck::Systemd) {
    InstallMonadoCheck(p);
    log("Installed the update check for Monado / WiVRn");
  } else {
    RemoveMonadoCheck();
  }
  const fs::path checkPlugin = ShortcutFile("update-check");
  if (options.monadoCheck == MonadoCheck::Envision) {
    if (EnableEnvisionPlugin(checkPlugin, true)) log("Added the update check to Envision's plugins");
    else log("Enable \"openhouse update check\" in Envision's Preferences (Envision is open, or not set up yet)");
  } else {
    EnableEnvisionPlugin(checkPlugin, false);
  }
#else
  (void)options;
#endif
}

void UninstallDriver(const fs::path& steamvr, const std::function<void(const std::string&)>& log) {
  const Paths p = MakePaths(steamvr);
  std::error_code ec;
  // The driver first: if anything after it fails, SteamVR still has its own driver back. (Steam's
  // appconfig.json held by Steam or a virus scanner used to stop uninstall before the driver.)
  RemoveMovedAside(p);
  if (!fs::exists(p.target, ec)) throw std::runtime_error("SteamVR's lighthouse driver isn't at " + Utf8(p.target) + ".");
  if (Contents(p.target).find(kMarker) == std::string::npos) {
    // A SteamVR update put its own, newer driver in place of ours: the backup is older, and
    // moving it back would undo the update, so it goes instead.
    RemoveFile(p.backup);
    log("SteamVR's own driver was already in place (a SteamVR update put it back)");
  } else {
    if (!fs::exists(p.backup, ec))
      throw std::runtime_error("SteamVR's own driver isn't kept next to openhouse's (" + Utf8(p.backup.filename()) +
                               "). Verify SteamVR's files in Steam (Properties > Installed Files) to get it back.");
    const std::string stock = Contents(p.backup);
    CopyOver(p.backup, p.target);
    // Checked before the backup goes: it's the only copy of SteamVR's driver.
    if (Contents(p.target) != stock)
      throw std::runtime_error("SteamVR's driver couldn't be put back over openhouse's. Close Steam and SteamVR completely, then try again.");
    RemoveFile(p.backup);
    log("Put SteamVR's own driver back");
  }
#ifndef _WIN32
  fs::remove_all(p.directory / "lh-libs", ec);
#endif
  Try(log, "Old settings page not removed", [&] { RemoveOldSettingsPage(p); });
  Try(log, "Old update check not removed", [&] { RemoveOldUpdateCheck(p, log); });
  Try(log, "Overlay not unregistered", [&] { SetRegistration(p, p.data / "openhouse-overlay.vrmanifest", kOverlayKey, false, log); });
#ifndef _WIN32
  RemoveMonadoCheck();
#endif
#ifndef _WIN32
  Try(log, "Envision plugin not removed", [&] { EnableEnvisionPlugin(ShortcutFile("update-check"), false); });
#endif
  Try(log, "Menu entries not removed", [&] { RemoveShortcuts(); });
  Try(log, "Link handler not removed", [&] { RemoveUrlHandler(); });
  // Only the files the installer put there: the folder also holds the user's recordings.
  for (const char* name : {"update-checker", "update-checker.exe", "update-checker.vrmanifest", "openhouse-installer", "openhouse-installer.exe", "openhouse-overlay",
                           "openhouse-overlay.exe", "openhouse-overlay.vrmanifest", "openhouse-monado-watch", "doorman-dongle-manager",
                           "doorman-dongle-manager.exe", kBleLibrary})
    RemoveFile(p.data / name);
  fs::remove_all(p.data / "icons", ec);
  fs::remove_all(p.data / "package", ec);
  fs::remove_all(p.data / "update", ec);
  fs::remove(p.data, ec);  // only goes when nothing else is in it
  log("Removed the update check and the dashboard overlay");
#ifndef _WIN32
  if (std::string error; !RemoveDeviceAccess(&error)) log("Device access rule not removed: " + error);
#endif
}

}  // namespace openhouse::installer
