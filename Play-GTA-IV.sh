#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if pgrep -x GTAIV.exe >/dev/null; then
  echo 'Close GTA IV before starting the bridge launcher.' >&2; exit 1
fi
bash bridge/start.sh
# Steam takes a moment to enumerate the virtual Series controller.
sleep 2
if ! xdg-open steam://rungameid/12210; then
  bash bridge/stop.sh
  exit 1
fi
echo 'GTA IV launch requested. After playing, use Stop-GTA-IV-HD_Rumble.sh.'
