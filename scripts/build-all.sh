#!/usr/bin/env bash
# Builds the firmwares once, then every platform package with them, like CI does.
# Output: dist/firmwares/ and the Linux and Windows packages in dist/.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"

python3 "$repo/doorman/tools/boards.py" check-cpp "$repo/src/common/doorman_boards.hpp"
python3 "$repo/scripts/stage-firmwares.py" "$repo/dist"
export OPENHOUSE_FIRMWARES="$repo/dist/firmwares"

"$repo/scripts/build-linux.sh" "$@"
"$repo/scripts/build-windows.sh" "$@"
