# Configuration reference

Edit `GTA-IV-HD_Rumble.ini` **next to the installed ASI**, not just the copy in this download. Settings are read at game startup. Use decimal points, not commas. Missing/malformed numeric settings use defaults; numeric values outside the listed range are clamped. Keep one section/key occurrence. Put comments on separate `;` lines.

## `[GTA-IV-HD_Rumble]`

| Key | Default | Range / meaning |
| --- | --- | --- |
| Profile | 1 | 0 Strong, 1 Medium, 2 Light |
| ControllerIndex | 0 | XInput slot 0–3 |
| SaveHashes | 1 | 1 writes F10 hashes, 0 disables writing |
| MainStrengthPercent | 100 | 0–400; overall vehicle gain, not shooting |
| MotorLimit | 1500 | 0–65535; final limit for each vehicle motor |
| TyreStrength | 300 | 0–65535; base strength per affected tyre side before gains/limit |

For vehicle output, effect gains are applied to individual contributions, which are summed, scaled by MainStrengthPercent and the current profile's StrengthPercent, then limited by MotorLimit. There is also internal saturation before final gains. Small commands may round to zero or be too weak to feel. Several burst tyres on the same side do not stack.

`MainStrengthPercent=0` or `MotorLimit=0` silences vehicle effects from this mod; neither is a global switch for stock game rumble. Trigger driving effects derive from this vehicle output and will also become zero.

## `[Effects]`

Each entry accepts **0–400 percent**, default **100**. **0 disables that scenario**; 50 halves it; 200 doubles its contribution, subject to the final limit.

| Key | Scenario |
| --- | --- |
| Acceleration | Positive speed change exceeding the profile threshold |
| Braking | Negative speed change exceeding the profile threshold |
| Gear | Larger speed drop with a cooldown; not a real transmission sensor |
| Bumps | Upward Z-coordinate change |
| Turns | Heading change |
| Damage | Loss of vehicle health |
| Tyres | Continuous burst-tyre rumble above the profile's minimum speed |

Disabling Acceleration/Braking also removes the corresponding driving trigger event. Disabling Gear does not disable Braking; both may contribute during a deceleration. Trigger driving strength uses the combined motor output on an acceleration/braking sample, so coincident effects may affect its strength.

## `[Strong]`, `[Medium]`, `[Light]`

| Key | Strong | Medium | Light | Range / units |
| --- | --- | --- | --- | --- |
| StrengthPercent | 100 | 100 | 100 | 0–400 percent |
| AccelerationThreshold | 0.06 | 0.08 | 0.10 | 0.001–10 m/s speed increase per 70 ms |
| BrakingThreshold | 0.06 | 0.08 | 0.10 | 0.001–10 m/s speed decrease per 70 ms; use a positive value |
| TyreMinSpeedKmh | 7.2 | 18 | 36 | 0–360 km/h; actual speed must be greater |

Acceleration and braking changes are normalized to the sample interval. Smaller thresholds detect gentler motion but can respond to coasting/noise. Other thresholds, vehicle coefficients and cooldowns remain in the source; they are not INI settings. The default profile names describe sensitivity/frequency, not stronger default motor power.

## `[Shooting]`

| Key | Default | Range / meaning |
| --- | --- | --- |
| OnFootPercent | 150 | 0–400; scale intercepted stock main-motor commands while shooting on foot |
| InVehiclePercent | 150 | 0–400; equivalent setting while in a vehicle |

100 preserves stock command strength, 150 applies ×1.5, 0 silences commands within the detected shooting window. Both motors saturate at 65535; vehicle MotorLimit does not apply here. The window lasts until 150 ms after the most recent detected shooting state. Other stock feedback occurring in that window may also be affected.

## `[Triggers]`

Requires the optional Linux bridge. These settings control trigger motors only; they do not rebind buttons or change main-motor recoil.

| Key | Default | Range / meaning |
| --- | --- | --- |
| Enabled | 1 | 0/1; disable/enable UDP trigger output |
| Acceleration | 1 | 0/1; RT during acceleration |
| Braking | 1 | 0/1; LT during deceleration |
| ShootingOnFoot | 1 | 0/1; RT recoil on foot |
| ShootingInVehicle | 0 | 0/1; RT recoil in vehicles; off because firing uses LB |
| StrengthPercent | 5 | 0–12; maximum trigger amplitude as a percent of full scale |
| DrivingPercent | 100 | 0–400; gain applied to driving-trigger contribution |
| ShootingPercent | 100 | 0–400; gain applied to shooting-trigger contribution |

Driving trigger base = twice the current larger vehicle motor command, then DrivingPercent and StrengthPercent cap. Shooting trigger base = one eighth of the larger stock motor command **after the Shooting gain**, then ShootingPercent and the trigger cap. Thus lowering stock shooting to 0 also removes trigger recoil. The independent bridge has a hard 12.5% safety ceiling.

In-car shooting temporarily suppresses driving effects even when ShootingInVehicle=0; this keeps vehicle feedback from overwriting stock recoil. After shooting ends, driving resumes. Stale driving/shot commands expire within 150/100 ms in the ASI; the bridge clears stale UDP commands after 250 ms. No trigger commands are sent to external hosts: the protocol uses only `127.0.0.1:27841`.

## Recipes

- Half all vehicle feedback: MainStrengthPercent=50.
- Softer tyres only: Effects/Tyres=50 (or halve TyreStrength).
- No bump feedback: Effects/Bumps=0.
- Earlier braking response: lower BrakingThreshold in the active profile.
- Trigger braking only: Triggers/Acceleration=0, ShootingOnFoot=0, ShootingInVehicle=0, Braking=1.
- Stock main-motor shooting: set both Shooting percentages to 100.
- Ordinary rumble without trigger output: Triggers/Enabled=0; launch without the bridge.

Restore the supplied INI to return to defaults, then restart the game.
