#!/usr/bin/env bash
set -euo pipefail
if systemctl --user is-active --quiet rumblenuts-bridge.service; then
  systemctl --user stop rumblenuts-bridge.service
fi
echo 'Bridge stopped. Reconnect USB if the normal controller does not return.'
