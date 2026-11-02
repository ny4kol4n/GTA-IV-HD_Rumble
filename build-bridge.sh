#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
# Requires a C++17 compiler, SDL3 development headers and SDL3 with direct GIP.
# On the tested Bazzite host SDL3 is 3.4.18. No private SDL runtime is bundled.
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Werror source/bridge.cpp \
  $(pkg-config --cflags --libs sdl3) -o bridge/rumblenuts-bridge
