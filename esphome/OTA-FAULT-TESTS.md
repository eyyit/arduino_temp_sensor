> Historical engineering note. Current deployed source and release status are
> described in README.md and SYSLOG-WEB-UI.md. Artifact/build-tool paths below
> refer to the separate deployment workspace, not files in this repository.

# OTA fault tests, October 23, 2026

Result: requested tests pass in the pinned Docker image; firmware compilation
and compiled LittleFS/OTA checks also pass. No physical device, network socket,
flash write, OTA upload or monitoring configuration was accessed by these tests.

## What is exercised

The harness extracts 13 OTA receiver method bodies unchanged from the pinned
ESPHome 2026.10.0 source. It compiles those bodies against simulated socket,
clock, update-backend and application/reboot interfaces. SHA256 uses the Docker
Python standard library as the host crypto adapter; only fixture credentials are
used. The real MCU SHA implementation, listener/complete wire handshake, native
TCP stack and physical flash backend are not tested by this harness.

The production OfficeOTAInterlock is shared by the Wi-Fi override and the test.
The test's OTA state-listener adapter mirrors the YAML begin/error/end hooks.
Generated-source checks independently confirm those three hooks and the native
password setter are compiled into the firmware.

- Wrong and empty client passwords are rejected with AUTH_INVALID; no backend
  begin/write occurs, no OTA-start callback fires, and the connection is cleaned.
- A fragmented correct response succeeds. Truncated auth closes cleanly. A valid
  response to an older server nonce is rejected. SHA256 capability is required.
- EOF at all 52 truncation boundaries of a short size/checksum/image fixture
  does not activate a new image or reboot. Multi-chunk truncations after1040,
  1560,8192 and9500 bytes likewise abort and never call successful end().
- A silent stalled peer reaches the receiver's105000ms inactivity timeout;
  simulated time advances without waiting105 real seconds. Socket reset and
  backend prepare/write/end errors abort, clean up and release the OTA interlock.
- New scans, use of roam-scan results and custom radio recovery are blocked while
  OTA is active. A scan already in progress can time out, but radio recovery is
  deferred until OTA ends/errors. Scanning remains blocked during radio reset,
  and becomes eligible afterward. These are production gate/policy tests, not a
  complete simulated ESP8266 SDK or full Wi-Fi state-machine execution.
- Success control: after every byte is received and backend end() succeeds,
  a missing final client acknowledgement is deliberately nonfatal upstream.
  Completion/reboot is expected; this is not reported as an interrupted upload
  that preserved the old image. Reboot is an exception in the test, not a device
  reboot. Activation/abort are backend observations, not physical-flash proof.

The previous sensor and roaming/callback fault suites also pass. Compilation
confirms the small Wi-Fi interlock refactor builds on ESP8266. The exact reserved
LittleFS addresses and generated OTA receiver/hooks remain verified.

## Files and reproduction

Run sh build.sh on the prepared Linux Docker workspace. All test dependencies,
compilation and checks run inside Docker; no host package installation is needed.
Tests: tests/ota_fault_test.cpp, tests/extract_ota_receiver.py.
Production interlock: components/wifi/office_ota_interlock.h.
Combined results: artifacts/ota-fault-build.log.
Binary: artifacts/sensor-office-OTA-FAULT-TESTED-COMPILE-ONLY.bin
SHA256: 7387dbebfd350005c0334c35ed483e1f98a7e9ff4454fb4e8504ae190e164e69
RAM33216/81920 bytes (40.5%); application flash386817/1044464 bytes (37.0%).

No full-device backup, real Wi-Fi provisioning, physical OTA password-rejection
test, power-loss/reboot test, or actual remote rollback acceptance is implied.
The compile-only image still has deliberately invalid Wi-Fi credentials. Those
deployment gates and the physical recovery route remain in OFFICE-BACKUP-RECOVERY.md.
