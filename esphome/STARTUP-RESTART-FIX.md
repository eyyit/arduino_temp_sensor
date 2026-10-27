> Historical engineering note. Current deployed source and release status are
> described in README.md and SYSLOG-WEB-UI.md. Artifact/build-tool paths below
> refer to the separate deployment workspace, not files in this repository.

# Office startup restart diagnosis and staged fix

## Windows serial access

The connected office USB bridge is CP2102, hardware instance
`USB\VID_10C4&PID_EA60\0001`. Windows initially reported missing-driver code 28.
The Silicon Labs download server returned access denied. Espressif's official
driver distribution supplied Silicon Labs driver 11.3.0.176. Its catalog had a
valid Microsoft Windows Hardware Compatibility Publisher signature and matched
the connected hardware. Following the Windows administrator approval, PnPUtil
installed it successfully as `oem122.inf`. The office board is now COM5.
No host firmware-build dependencies were installed. This is a Windows device
driver needed for serial access; all firmware tests and builds stayed in Docker.

## Diagnosis

Serial output at 115200 baud confirms MAC 5C:CF:7F:92:EA:79, the office device.
The user pressed RST once while capture was active. Logs show repeated normal
setup completion, software resets (`rst cause:2`), and increasing unsuccessful
boot counters. This was not an exception stack requiring address decoding.

The cause is the daily restart automation: `interval: 24h` did not delay its
first execution for 24 hours. In the exact pinned ESPHome 2026.10.0 runtime,
`Scheduler::calculate_interval_offset_()` chooses the first callback at
`now + random_offset`, where the offset is limited to 0..4999 ms, regardless of
the 24-hour repeating interval. That callback executes `ESP.restart()`.
Repeated early resets prevent the 60-second successful-boot threshold and
eventually enter the 5-minute recovery mode. In recovery mode the daily restart
automation is not registered, explaining stable Wi-Fi/OTA without HTTP/sensors.

Credential-free evidence: `artifacts/startup-reset-evidence.txt`.
The complete raw capture remains under ignored `.private/`; do not publish it.
The Windows serial read was canceled after capturing 50,415 bytes. The partial
capture contains enough repeated startup/reset cycles to establish the fault.

## Staged correction and regression coverage

`sensor-office.yaml` now sets `startup_delay: 24h` on the daily restart interval.
On each boot the first restart waits at least a full day, plus the scheduler's
small initial offset; later ticks retain the 24-hour interval.

The regression test compiles the actual pinned IntervalTrigger header, extracts
the unchanged PollingComponent startup/poller methods and scheduler offset
calculation, and uses a simulated clock/scheduler adapter. It reproduces the
released early restart and verifies the correction at minimum/middle/maximum
random offsets, the 60-second boot-success threshold, just before/at the first
daily tick, subsequent daily cadence, and crossing 32-bit uptime rollover.
It is a host simulation test, not proof of successful hardware operation.

Release checks now reject a missing/short startup delay in both YAML and the
generated C++ timer setters. Mutations cover absent/zero/short delays and a
setter attached to the wrong compiled timer. All eight release-check groups
passed. Existing sensor, roaming and OTA fault suites also passed.

Docker build and compiled filesystem/layout checks passed:

- Candidate: `artifacts/sensor-office-STARTUP-DELAY-CANDIDATE.bin`
- SHA256: `1d985433f94669a652af6f6e22a1d11c443204d0f00b3c85c8ddfe520a01b693`
- Size: 390,928 bytes
- RAM: 33,168 / 81,920 bytes (40.5%)
- Flash: 386,769 / 1,044,464 bytes (37.0%)
- LittleFS remains reserved at 0x300000..0x3FA000 exclusive.
- Build/test evidence: `artifacts/startup-delay-build.log`.

The originally installed firmware's matching ELF and generated source were
preserved privately before the rebuild. Both old and new ELF/images have private
persistent release copies on the server.

## Deployment verified

The user approved uploading the corrected pilot. Authenticated native ESPHome OTA succeeded. Serial logs confirmed successful boot, and 13 runtime polls over 84.36 seconds passed with fresh readings and increasing uptime. Later monitoring verification confirmed over 31,000 seconds uptime. Prometheus and Grafana were subsequently configured with explicit user authorization. See DEPLOYMENT-STATUS.md and OFFICE-MONITORING.md.
