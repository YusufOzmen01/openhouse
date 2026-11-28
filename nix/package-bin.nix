# openhouse from the GitHub release (nix/release.json: its version and the Linux tarball's hash;
# scripts/bump-nix-release.sh updates it): the binaries every Linux user gets, with each one's
# interpreter and libraries pointed at the Nix store (autoPatchelfHook). Nothing is compiled.
{
  lib,
  stdenv,
  fetchurl,
  autoPatchelfHook,
  copyDesktopItems,
  makeDesktopItem,
  systemd,
  dbus,
  libGL,
  libX11,
  libxcb,
  libXau,
  libXdmcp,
  freetype,
  zlib,
  bzip2,
  libpng,
  brotli,
  wayland,
  libxkbcommon,
  libdecor,
  libXcursor,
  libXrandr,
  libXi,
  libXinerama,
  libXext,
  repo,
}:

let
  release = lib.importJSON ./release.json;
  common = import ./common.nix { inherit lib makeDesktopItem; inherit (release) version; };
in
stdenv.mkDerivation {
  pname = "openhouse-bin";
  inherit (release) version;

  src = fetchurl {
    url = "https://github.com/YusufOzmen01/openhouse/releases/download/v${release.version}/openhouse-linux-x64.tar.gz";
    inherit (release) sha256;
  };

  nativeBuildInputs = [ autoPatchelfHook copyDesktopItems ];
  # What the release binaries link: the C++ runtime (the tools and the driver), udev, OpenGL and X11
  # (GLFW is built in), freetype and its libraries, and D-Bus (the Bluetooth library).
  buildInputs = [ stdenv.cc.cc.lib systemd dbus libGL libX11 libxcb libXau libXdmcp freetype zlib bzip2 libpng brotli ];
  # What GLFW opens itself (dlopen), so not linked: the Wayland and X11 libraries and OpenGL. Without them in
  # the rpath every window app stops with "no display".
  runtimeDependencies = [ libGL wayland libxkbcommon libdecor libX11 libXcursor libXrandr libXi libXinerama libXext ];

  dontConfigure = true;
  dontBuild = true;
  # The release's binaries are stripped already.
  dontStrip = true;

  installPhase = ''
    runHook preInstall
    share=$out/share/openhouse
    repo=${repo}
    mkdir -p $share
    cp -r . $share/
    ${common.installCommon}
    runHook postInstall
  '';

  inherit (common) desktopItems;
  meta = common.meta // { sourceProvenance = [ lib.sourceTypes.binaryNativeCode ]; };
}
