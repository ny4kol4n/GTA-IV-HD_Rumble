#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
bridge_dir="$(readlink -f "$PWD")"
: "${XDG_RUNTIME_DIR:?Run this from your normal host desktop session}"
ready_file="$XDG_RUNTIME_DIR/rumblenuts-bridge.ready"
if systemctl --user is-active --quiet rumblenuts-bridge.service; then
  bridge_pid=$(systemctl --user show -p MainPID --value rumblenuts-bridge.service)
  if [[ -f "$ready_file" && "$(cat "$ready_file")" == "$bridge_pid" ]] && [[ "$(readlink -f "/proc/$bridge_pid/exe")" == "$bridge_dir/rumblenuts-bridge" ]]; then
    echo 'Bridge already ready. Steam Input stays ON.'; exit 0
  fi
  echo 'Another/older bridge is running. Close the game, run its stop.sh, then retry.' >&2
  exit 1
fi
rm -f -- "$ready_file"
systemd-run --user --unit=rumblenuts-bridge --collect \
  --property=TimeoutStopSec=5 \
  --property="StandardOutput=append:$bridge_dir/bridge.log" \
  --property="StandardError=append:$bridge_dir/bridge.log" \
  "$bridge_dir/rumblenuts-bridge"
for ((attempt=0; attempt<100; attempt++)); do
  bridge_pid=$(systemctl --user show -p MainPID --value rumblenuts-bridge.service 2>/dev/null || true)
  if [[ "$bridge_pid" != 0 && -n "$bridge_pid" && -f "$ready_file" && "$(cat "$ready_file")" == "$bridge_pid" ]]; then
    echo 'Bridge READY. Keep Steam Input ON.'; exit 0
  fi
  sleep 0.1
done
systemctl --user stop rumblenuts-bridge.service || true
echo 'Bridge not ready. See bridge.log. Check USB/uinput permissions and SDL3.' >&2
exit 1
