#!/usr/bin/env python3
"""Create portable-path desktop shortcuts after moving/extracting the package."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent
result = subprocess.run(['xdg-user-dir', 'DESKTOP'], capture_output=True, text=True)
if result.returncode != 0 or not result.stdout.strip():
    raise SystemExit('Cannot locate desktop directory. Run Play-GTA-IV.sh manually.')
desktop = Path(result.stdout.strip())
desktop.mkdir(parents=True, exist_ok=True)

def exec_quote(text):
    # Desktop Entry Exec uses its own quoting and percent field codes, not a shell.
    return '"' + str(text).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%') + '"'

for filename, title, script, comment in [
    ('GTA-IV-HD_Rumble-Play.desktop', 'GTA IV + GTA-IV-HD_Rumble', 'Play-GTA-IV.sh', 'Start the trigger bridge, then launch GTA IV through Steam'),
    ('GTA-IV-HD_Rumble-Stop.desktop', 'Stop GTA-IV-HD_Rumble bridge', 'Stop-GTA-IV-HD_Rumble.sh', 'Stop the bridge after closing GTA IV')
]:
    target = desktop / filename
    target.write_text('[Desktop Entry]\nType=Application\nVersion=1.0\n'
                      f'Name={title}\nComment={comment}\n'
                      f'Exec=/usr/bin/bash {exec_quote(root / script)}\n'
                      'Icon=input-gaming\nTerminal=true\nCategories=Game;\n', encoding='utf-8')
    target.chmod(0o755)
    print(target)
print('If your desktop asks, allow launching these shortcuts. Re-run after moving the folder.')
