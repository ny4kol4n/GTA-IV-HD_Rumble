#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if pgrep -x GTAIV.exe >/dev/null; then
  echo 'Close GTA IV before stopping the input bridge.' >&2; exit 1
fi
bash bridge/stop.sh
