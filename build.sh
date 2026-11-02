#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
compiler="${CXX:-i686-w64-mingw32-g++}"
"$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-cast-function-type -shared -static -static-libgcc -static-libstdc++ source/plugin.cpp -o GTA-IV-HD_Rumble.asi -lversion -luser32 -lws2_32 -Wl,--kill-at -Wl,--no-insert-timestamp
