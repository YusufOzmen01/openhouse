# openhouse built from source with nixpkgs' libraries, laid out as the Linux package
# (share/openhouse: the installer, driver_lighthouse.so, apps/, firmwares/), so the installer
# and the apps find each other as they do there. Everything points straight at its Nix libraries:
# nothing needs /lib64/ld-linux or steam-run.
{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  python3,
  gcc-arm-embedded,
  autoPatchelfHook,
  copyDesktopItems,
  makeDesktopItem,
  fetchurl,
  eigen,
  ceres-solver,
  nlohmann_json,
  zlib,
  systemd,
  dbus,
  openvr,
  glfw,
  freetype,
  libGL,
  wayland,
  libxkbcommon,
  libdecor,
  libX11,
  libXcursor,
  libXrandr,
  libXi,
  libXinerama,
  libXext,
  version,
  commit ? "",
  src,
  # The firmware the installer flashes (base stations, Doorman dongles and radios): an ARM
  # cross toolchain build, off for a quicker build of the PC side only.
  withFirmware ? true,
}:

let
  # ShayBox's Lighthouse library (the overlay's Bluetooth channel changes), pinned as
  # scripts/fetch-lighthouse-ble.py pins it.
  lighthouseBle = fetchurl {
    url = "https://github.com/ShayBox/Lighthouse/releases/download/2.1.0/liblighthouse-x86_64-unknown-linux-gnu.so";
    sha256 = "2856023bc93f72a48731865b281cb35fa530088e78d8a9fb6319e15864dc1577";
  };
  # Without BLAS, METIS and SuiteSparse, as the release packages' Ceres (vcpkg, "schur"): no GPL
  # code in the MIT driver.
  ceres = ceres-solver.override { withBlas = false; };
  common = import ./common.nix { inherit lib makeDesktopItem version; };
  apps = common.apps;
in
stdenv.mkDerivation {
  pname = "openhouse";
  inherit version src;

  nativeBuildInputs = [ cmake ninja pkg-config python3 autoPatchelfHook copyDesktopItems ]
    ++ lib.optionals withFirmware [ gcc-arm-embedded ];
  buildInputs = [ eigen ceres nlohmann_json zlib systemd dbus openvr glfw freetype libGL ];
  # What GLFW opens itself (dlopen), so not linked: without them in the rpath, "no display" (package-bin.nix).
  runtimeDependencies = [ libGL wayland libxkbcommon libdecor libX11 libXcursor libXrandr libXi libXinerama libXext ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DLH_BUILD_TESTS=OFF"
    "-DLH_BUILD_ORACLE_TESTS=OFF"
    "-DLH_BUILD_TOOLS=ON"
    "-DLH_BUILD_VIEWER=ON"
    "-DLH_STATIC_GLFW=OFF"
    # Nix gives every binary its libraries' store paths; none keeps the build directory.
    "-DCMAKE_SKIP_BUILD_RPATH=ON"
  ];

  # The version stamp and the firmware's file name (cmake/build_id.cmake, stage-firmwares.py).
  env.OPENHOUSE_BUILD_COMMIT = commit;

  postBuild = lib.optionalString withFirmware ''
    (cd .. && python3 scripts/stage-firmwares.py "$NIX_BUILD_TOP/stage")
  '';

  installPhase = ''
    runHook preInstall
    share=$out/share/openhouse
    mkdir -p $share/apps/icons
    install -m755 openhouse-installer $share/
    install -m755 openvr/lighthouse/bin/linux64/driver_lighthouse.so $share/
    install -m644 VERSION.txt $share/
    install -m644 ../LICENSE $share/LICENSE.txt
    for app in ${lib.concatStringsSep " " apps}; do install -m755 $app $share/apps/; done
    install -m644 ../cmake/linux/openhouse.png ../cmake/linux/dongle-manager.png $share/apps/icons/
    install -m755 ${lighthouseBle} $share/apps/liblighthouse.so
    ${lib.optionalString withFirmware "cp -r $NIX_BUILD_TOP/stage/firmwares $share/"}
    repo=..
    ${common.installCommon}
    runHook postInstall
  '';

  # The installed driver must outlive this package: SteamVR's copy is outside the store, so a
  # garbage-collected update would take its libraries away. As in the release package
  # (scripts/package-driver.py), the driver's own libraries go next to it in lh-libs; the C and
  # C++ runtimes come from the process that loads it. After autoPatchelf (postFixup), which would
  # point it back at the store.
  postFixup = ''
    share=$out/share/openhouse
    mkdir -p $share/lh-libs
    bundle() {
      local f=$1 name path
      for name in $(patchelf --print-needed "$f"); do
        case "$name" in libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*|ld-linux*|libstdc++.so*|libgcc_s.so*) continue;; esac
        [ -e "$share/lh-libs/$name" ] && continue
        path=""
        for dir in $(patchelf --print-rpath "$f" | tr : ' '); do
          [ -e "$dir/$name" ] && { path="$dir/$name"; break; }
        done
        [ -n "$path" ] || { echo "lh-libs: $name not found for $f"; exit 1; }
        install -m755 "$(readlink -f "$path")" "$share/lh-libs/$name"
        bundle "$share/lh-libs/$name"
        patchelf --set-rpath '$ORIGIN' "$share/lh-libs/$name"
      done
    }
    bundle $share/driver_lighthouse.so
    patchelf --set-rpath '$ORIGIN/lh-libs' $share/driver_lighthouse.so
  '';

  inherit (common) desktopItems meta;
}
