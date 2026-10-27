> Historical engineering note. Current deployed source and release status are
> described in README.md and SYSLOG-WEB-UI.md. Artifact/build-tool paths below
> refer to the separate deployment workspace, not files in this repository.

# Office pilot: sensor recovery and freshness

Compile-only change, October 23, 2026. No flashing, device configuration,
Prometheus/Grafana changes, service restarts or host package installations.

LATEST: OTA-LITTLEFS.md describes the newer authenticated-OTA build with the old
filesystem reservation. Earlier no-OTA/layout comments below are historical;
sensor recovery/freshness settings remain unchanged.

## Current behavior

- Local bme280_base/bme280_i2c override is based on pinned ESPHome 2026.10.0.
  Upstream archive: artifacts/upstream-bme280.tar; the ESPHome license is retained
  in components/wifi/UPSTREAM-LICENSE.
- Hardware/settings: GPIO4/GPIO5, 100kHz, 0x76, forced mode, 1x on all channels,
  IIR OFF, five-second sensor poll, 1020hPa altitude reference, existing dew-point
  math. No calibration offset or replacement hardware setting was invented.
- Startup failure stays retryable rather than permanently failing the component.
  Each subsequent poll retries full setup: ID, soft reset, bounded NVM wait,
  calibration, humidity/config registers, then a complete conversion.
- Every calibration transaction is checked. Any read failure or zero p1 rejects
  initialization instead of allowing measurements with partial coefficients.
- Conversion-write/read/status failure immediately invalidates all three raw
  entities. Publishing RH last invokes the YAML callback to invalidate all derived
  entities too. The full compensated frame must be finite and within sensor
  operating ranges before any valid values publish: -40..85C, 300..1100hPa,
  0..100%RH. A busy/resetting conversion does not count as a fresh sample.
- A separate loop guard invalidates readings at 15 seconds without success and
  cancels pending conversion work. Recovery requires full setup plus a valid
  frame, not just an I2C acknowledgement. This requires a running main loop and
  cannot protect against an entirely hung MCU.
- Sensor Sample Fresh reports 0/1; Sensor Sample Age reports seconds since the
  last successful frame, or NaN before the first. Both refresh every second.
- All configured numeric sensor entities use accuracy_decimals: 2. The standard
  exporter omits NaN values and emits esphome_sensor_failed=1. Firmware identity
  metrics are not sensor entities and are not reformatted by this setting.
- Prometheus lookback can briefly retain historical values after a series
  disappears. Future queries should use failed/freshness telemetry. No query or
  dashboard changes are part of this task.

## Wi-Fi overlap

wifi_scan_start_ refuses requests unless the scan watchdog is IDLE. The watchdog
also rejects a second started() call, starts during reset, and starts after
callback-generation exhaustion. The original request/generation/deadline remain
intact. Reconnection waits for the outstanding callback or watchdog recovery.
The existing 64 non-recycled callback generations and SSID-only exhaustion fallback
remain. Their safety/capacity limits are described in components/wifi/OFFICE-PILOT.md.

## Validation

artifacts/sensor-fault-tests.log and artifacts/sensor-recovery-build.log pass.
Tests compile the modified production bme280_base.cpp against simulated I2C,
clock, entities and timeout scheduling. The pinned upstream Prometheus sensor-row
method is tested unchanged with stream/label/number-format doubles.

Covered: absent boot/recovery; every calibration transaction; setup register
faults; wrong chip ID; zero pressure coefficient; conversion/read/status failure;
reset-lost humidity configuration; invalid ADC sentinels; busy conversion;
15-second stale cutoff and timer rollover; cancellation of pending readings;
derived invalidation; exporter omission/failure flag; two-decimal formatting.
Wi-Fi tests cover overlap refusal, preserved deadlines, reset exclusion, retired
callbacks, timeout recovery and callback-generation exhaustion.

These are software fault tests, not actual HTTP socket, electrical I2C, sensor
power-cycle, radio, or hardware scheduler tests. Compilation does not prove those.

Latest binary: artifacts/sensor-office-SENSOR-RECOVERY-COMPILE-ONLY.bin
SHA256: 1364c85d96c96d77d1488376ffed7909bc097f78ed5418547e9021574ac20c76
RAM: 32824/81920 bytes (40.1%). Application flash: 369993/1044464 bytes (35.4%).

## Still required before flashing

UPDATE: RELEASE-CANDIDATE.md closes the test-Wi-Fi provisioning/rebuild item
below. Full backup and physical acceptance gates remain open.

- The binary contains deliberately invalid Wi-Fi test credentials. Supply real
  provisioning outside Git and rebuild before deployment.
- Choose a flashing/recovery method. This pilot has no OTA component. Compatibility
  with the old firmware's OTA/filesystem layout has not been established; do not
  assume old credentials/settings will be understood. Preserve a recoverable
  backup before any separately authorized flash.
- Hardware fault/soak acceptance remains pending: sensor unplug/reconnect and
  power cycling, actual /metrics freshness/failure responses, radio recovery and
  SDK callback behavior, and prolonged operation. Those checks need a separately
  authorized bench session; some necessarily follow an initial bench flash.
- Existing daily restart and ESPHome connection-loss reboot fallback remain.

No production flashing has been authorized by this task.
