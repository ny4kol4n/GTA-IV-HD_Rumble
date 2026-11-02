"""Test launch ordering/failure handling without starting Steam or touching input."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    folder = Path(temp)
    binaries = folder / 'bin'
    binaries.mkdir()
    log = folder / 'calls'
    programs = {
        'pgrep': '#!/bin/sh\nexit "${GAME_RUNNING:-1}"\n',
        'sleep': '#!/bin/sh\nexit 0\n',
        'bash': '#!/bin/sh\necho "$1" >> "$CALLS"\ncase "$1" in *start.sh) exit "${BRIDGE_RESULT:-0}";; esac\nexit 0\n',
        'xdg-open': '#!/bin/sh\necho "$1" >> "$CALLS"\nexit "${STEAM_RESULT:-0}"\n',
    }
    for name, content in programs.items():
        path = binaries / name
        path.write_text(content)
        path.chmod(0o755)
    cases = [
        ({}, 0, ['bridge/start.sh', 'steam://rungameid/12210']),
        ({'BRIDGE_RESULT': '1'}, 1, ['bridge/start.sh']),
        ({'GAME_RUNNING': '0'}, 1, []),
        ({'STEAM_RESULT': '1'}, 1, ['bridge/start.sh', 'steam://rungameid/12210', 'bridge/stop.sh']),
    ]
    for overrides, expected_status, expected_calls in cases:
        log.write_text('')
        env = dict(os.environ, PATH=str(binaries)+':'+os.environ['PATH'], CALLS=str(log), **overrides)
        result = subprocess.run(['/bin/bash', str(root / 'Play-GTA-IV.sh')], env=env, capture_output=True, text=True)
        assert result.returncode == expected_status, result.stderr
        assert log.read_text().splitlines() == expected_calls
print('PASS: readiness gating, running-game guard, Steam launch and failure cleanup (mocked).')
