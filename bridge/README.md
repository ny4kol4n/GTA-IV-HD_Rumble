# Linux impulse-trigger bridge

## Requirements

- USB Xbox Series controller **045e:0b12**, one matching physical device.
- Linux x86_64 for the bundled executable, systemd user services, Steam with Steam Input enabled.
- SDL3 with direct GIP/libusb support. Tested host runtime: SDL3 3.4.18 on Bazzite. Rebuild for incompatible distributions.
- Read/write access to the controller's USB device node **and `/dev/uinput`** from your logged-in desktop user.
- Host tools: bash, systemctl/systemd-run, pgrep, xdg-open; Python 3 and xdg-user-dir for creating shortcuts.

The bridge does not install drivers, change Steam settings, add login autostart, or elevate privileges. It temporarily takes the physical USB interface from xpad and forwards buttons/sticks/triggers to a virtual **Xbox Series** device. Steam Input reads the virtual controller. Ordinary rumble returns through virtual force feedback; the ASI sends impulse-trigger commands over loopback UDP.

## Device permissions

On the tested PC, USB and uinput access were already configured. Other systems may need administrator setup. Do **not** run the bridge as root and do not make all input/USB devices world-writable.

An administrator can place the supplied `72-rumblenuts-controller.rules` in `/etc/udev/rules.d/`, reload udev rules and reconnect the controller. These are optional setup instructions, **not steps performed by the launcher**. The USB rule is restricted to 045e:0b12. The uinput rule grants the active local user permission to create virtual input devices, a broader capability required by this bridge. If your system already manages these permissions, keep its existing policy.

After rule installation, `/dev/uinput` may need its udev event retriggered or a reboot; the uinput kernel module must exist. Active-session ACL behavior is distribution-dependent. To undo only these supplied rules, remove that exact rule file, reload rules and reconnect/reboot. Do not remove other controller rules.

## Run

From the package root, on the **host desktop**:

```bash
bash bridge/start.sh
```

The script waits for a readiness file containing the current service PID before reporting success. The service is temporary: `rumblenuts-bridge.service`. It creates no login autostart. If a different/older bridge is already running, close the game, stop that bridge, then retry.

After READY, launch GTA IV normally, or use `Play-GTA-IV.sh` to perform both steps. Stop after closing the game with `Stop-GTA-IV-HD_Rumble.sh`. A desktop may require “Allow launching” for generated shortcuts.

For a manual stop:

```bash
bash bridge/stop.sh
```

Do not stop the bridge during gameplay: the virtual controller disappears. Normal shutdown zeros motors, destroys the virtual device and requests reattachment to xpad. Reconnect the USB cable if the physical controller is missing after a crash/forced kill. Ordinary ASI vibration can still run without the bridge; triggers cannot.

## Troubleshooting

- `bridge.log`: READY, forwarded-input frames, UDP packet counts and instantaneous motor levels.
- ASI `GTA-IV-HD_Rumble.log`: native discovery, XInput status, shot-rumble hook and UDP sender readiness.
- Packet counts prove commands arrived, not that motors physically moved. Check acceleration/RT, braking/LT and on-foot shooting/RT separately.
- Check A/B/X/Y, sticks, Guide and Share in Steam after switching identities. Steam can associate different bindings with the virtual controller.
- No controller: stop bridge and reconnect USB. Do not disable Steam Input as a workaround.
- No ordinary rumble: inspect Steam rumble settings and the bridge's feedback route. No trigger rumble: inspect INI toggles, trigger ceiling, UDP sender and bridge status.
- Port 27841 already occupied: stop the other bridge instance. Listener accepts only loopback; it is not intended for remote control.
- No GIP capability or SDL load error: use a compatible SDL3 runtime/rebuild; the bridge refuses to replace input if trigger support is absent.

## Build

From the package root: `bash build-bridge.sh` with a C++17 compiler, SDL3 development package and pkg-config. No third-party runtime libraries are bundled. Physical Bluetooth/dongle access and additional controller models are not implemented.

### Very weak main rumble

SDL's USB GIP backend quantizes motor strength in steps of 655 out of 65535. The bridge raises nonzero main-motor requests below 655 to that minimum so weak driving and burst-tyre effects are not rounded to silence while triggers remain active. Zero still stops the motor. Trigger strength and stronger main-motor requests are unchanged. Values below this hardware step cannot produce distinct steady strengths through this backend.

### Automatic main-motor muting while driving

While fresh acceleration or braking trigger effects are active, the bridge mutes both main motors. This includes other simultaneous main-motor effects such as burst tyres or bumps. Main feedback resumes when the driving triggers stop or their commands expire (250 ms). On-foot shooting keeps main and trigger rumble together. Without the bridge, normal XInput vehicle rumble remains unchanged; no INI edits are needed.

Install the updated ASI and bridge together: the 12-byte trigger packet identifies driving effects. The bridge also accepts old 8-byte packets without automatic muting, since those do not identify the effect type.
