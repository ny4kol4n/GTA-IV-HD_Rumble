# Validation — 1.0

## Automated / directly checked

- 32-bit ASI built with MinGW-w64 C++17, warnings treated as errors.
- Linux x86_64 bridge rebuilt against system SDL3; no bundled private runtime libraries.
- Engine tests passed: baseline, vehicle switching, repair, angle wrap, stale samples, boats/motorcycles, profiles, continuous tyres and caps, gradual acceleration and 90 km/h-to-zero braking at 70/140 ms intervals.
- Config tests passed: invalid/non-finite inputs, numeric bounds, effect disable, main gain, motor ceiling, tyre strength, all effects disabled, configurable shooting scaling and saturation. Default reference outputs remain unchanged.
- All 20 native hashes match the local FusionFix source reference. This is not a complete ABI/runtime test.
- Launcher scripts pass shell syntax checks. Mocked tests verify startup ordering, refusal while the game is running, no Steam launch when bridge startup fails, and stop-on-launch-failure.
- On the actual Bazzite host, bridge startup reached READY; repeated startup recognized the same service; stop requested kernel reattachment and xpad was bound again. No game was launched for this test.
- Checksums cover the two supplied executables. Personal logs, backup binaries and game files are not packaged.

## Confirmed functionality in earlier builds

Impulse-trigger functionality is confirmed on USB Xbox Series with Steam Input enabled. A swapped X/Y mapping was subsequently corrected. The 0.5.2 braking fix is confirmed to work. Steam logs confirmed the virtual Series identity and Share mapping; the Share action itself was not explicitly confirmed.

## Not yet verified

All combinations of settings, the complete desktop-to-game launch, and physical motor behavior with the renamed 1.0 binaries need an in-game check. Automated test success and packet counts do not prove physical feedback. Bluetooth, wireless dongles, other controller models, other game builds and other distributions are unverified.

## Steam Deck verification and 1.0 naming

Ordinary rumble is confirmed to work on Steam Deck in Gaming Mode without the trigger bridge. Version 1.0 renames the project, ASI/INI filenames, main INI section and desktop shortcuts; effect tuning is unchanged. This verification does not cover impulse triggers on Steam Deck or all configuration combinations.
