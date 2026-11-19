#!/usr/bin/env bash
# openhouse-steamvr register|unregister [DRIVER_DIR]
# Lists DRIVER_DIR as an external driver in openvrpaths.vrpath and turns SteamVR's own
# lighthouse driver off (steamvr.vrsettings). Close SteamVR first. Needs jq.
set -euo pipefail

action=${1:-}
dir=${2:-${OPENHOUSE_DRIVER_DIR:-/usr/share/openhouse}}
vrpath=${XDG_CONFIG_HOME:-$HOME/.config}/openvr/openvrpaths.vrpath
settings=${OPENHOUSE_STEAM_CONFIG:-$HOME/.local/share/Steam/config}/steamvr.vrsettings

# edit FILE JQ_ARGS...: filter FILE in place (jq finishes before the file is rewritten)
edit() {
  local file=$1 out
  shift
  [[ -f $file ]] || { mkdir -p "${file%/*}"; echo '{}' > "$file"; }
  out=$(jq "$@" "$file")
  printf '%s\n' "$out" > "$file"
}

case $action in
  register)
    edit "$vrpath" --tab --arg d "$dir" \
      '.external_drivers = ((.external_drivers // []) - [$d] + [$d])'
    edit "$settings" --indent 3 '.driver_lighthouse.enable = false'
    ;;
  unregister)
    edit "$vrpath" --tab --arg d "$dir" \
      '.external_drivers = ((.external_drivers // []) - [$d])
       | if .external_drivers == [] then del(.external_drivers) else . end'
    edit "$settings" --indent 3 \
      'del(.driver_lighthouse.enable)
       | if .driver_lighthouse == {} then del(.driver_lighthouse) else . end'
    ;;
  *)
    echo "usage: ${0##*/} register|unregister [DRIVER_DIR]" >&2
    exit 2
    ;;
esac
