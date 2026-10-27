> Historical engineering note. Current deployed source and release status are
> described in README.md and SYSLOG-WEB-UI.md. Artifact/build-tool paths below
> refer to the separate deployment workspace, not files in this repository.

# Reserved LittleFS and authenticated OTA, October 23, 2026

Prepared and compiled only. No device upload, flashing, reset, monitoring edits,
service restarts or host package installs.

## Implemented

- board_build.ldscript=eagle.flash.4m1m.ld. Compiled ELF _FS_start is 0x40500000
  and referenced _SPIFFS_end is 0x405FA000: physical LittleFS reservation
  0x300000..0x3FA000 exclusive, matching the archived old firmware exactly.
  EEPROM at0x3FB000 and RF/SDK tail sectors remain as before.
- No filesystem upload, mount, format or conversion is enabled. ESPHome does not
  consume the old LittleFS files; reserving their space is not settings migration.
- Native ESPHome OTA is enabled with a newly generated 256-bit random password
  referenced through !secret ota_password. It is stored in ignored secrets.yaml
  locally and in the private Linux build configuration, not in this note or Git.
  The password is distinct from the old ArduinoOTA password. Keep it available
  to the operator. secrets.example.yaml contains only a replacement placeholder.
- HTTP web-server OTA remains explicitly disabled. No Home Assistant API, MQTT,
  Graphite push or monitoring configuration was introduced.
- OTA begin/end/error callbacks coordinate with the local Wi-Fi override. During
  a transfer no new scan is requested and custom scan-watchdog radio recovery is
  deferred. Completed scan results are discarded instead of triggering roaming.
  A timed-out pending scan recovers after the OTA session ends/errors. The SDK
  cannot cancel an already-running scan, so brief overlap at upload start is
  possible. Do not claim atomic scan/OTA exclusion or tested RF behavior.

## Validation and artifacts

LATEST: OTA-FAULT-TESTS.md supersedes the former lack of software fault coverage
below. Pinned receiver methods now have simulated authentication/interruption
tests and the production roaming interlock is exercised. Physical OTA behavior
and rollback are still untested. Latest log: artifacts/ota-fault-build.log.

Docker build and existing sensor/Wi-Fi fault tests pass: artifacts/ota-build.log.
Actual ELF/generated-source checks pass: artifacts/ota-layout-check.log.
tests/verify_ota_layout.py verifies exact filesystem reservation, 4m1m link flag,
image size, generated OTA password setter, native receiver and all three hooks.
build.sh now runs the fault tests, compiles, then performs this layout check.
No actual OTA session or password-rejection test has been run against hardware.

RAM33216/81920 bytes (40.5%); application flash386817/1044464 bytes (37.0%).
Latest binary: artifacts/sensor-office-OTA-COMPILE-ONLY.bin
SHA256: 8479e57bb444e396b004532d22af7927329b2ee9dfdb14e227e4b663635696a6
The image still contains deliberately invalid Wi-Fi test credentials. Do not flash
this compile-only image. Rebuild with real Wi-Fi credentials before deployment.

## Remote rollback scope

Once a properly provisioned pilot is installed and reachable, its native ESPHome
OTA receiver provides an authenticated firmware-upload path. Use an ESPHome OTA
client matching this build and its new password, not Arduino espota. Uploading
the archived old application is the intended remote rollback path; the reserved
LittleFS area and unpinned Wi-Fi SDK configuration allow the old application's
settings to remain available if those sectors have not otherwise changed.

After rollback, the old ArduinoOTA implementation/password takes over again.
This is a whole application replacement, not a dual-slot automatic rollback or
a complete flash restore. It depends on working Wi-Fi, receiver/password access,
firmware compatibility and an uninterrupted successful update. It does not restore
LittleFS/SDK sectors if damaged and cannot rescue an unreachable/nonbooting MCU.

The old binary fits comfortably within this layout. Header-mode differences and
actual transition/rollback must still be verified in an authorized bench session.
Retain the complete serial flash backup and physical recovery route described in
OFFICE-BACKUP-RECOVERY.md. That full device backup is still pending.

Earlier notes saying the current pilot has no OTA or no reserved filesystem are
historical and are superseded by this build.
