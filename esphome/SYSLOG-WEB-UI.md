# Syslog and local sensor web interface

Deployed and verified on office, bedroom, livingroom, outside and kid on October
24, 2026. All five retain fresh climate readings, live heap diagnostics and healthy
five-second Prometheus scraping.

## Logging

Native ESPHome syslog sends INFO, WARN and ERROR events to **10.0.0.2:514/UDP**,
using RFC5424 and the sensor's existing hostname. Each device's connection marker
and/or successful-boot event was found in the existing receiver's
**/var/log/syslog**. No receiver configuration or service restart was necessary.

Global logging is INFO, avoiding routine measurement/debug traffic. Useful weak
signal scan and successful roaming events were promoted to INFO, scan-start
failure is WARN, and the first valid BME280 frame after startup/failure emits an
availability event. Sensor recovery, freshness, scan handling and OTA behavior
otherwise retain their existing logic.

SNTP uses ntp.ubuntu.com, matching the existing Docker host's upstream time source.
The firmware clock uses UTC. RFC5424 can send events before the clock is synchronized;
the receiver records arrival time. UDP logs remain best effort with no persistent
queue, acknowledgements or flash writes. USB serial logging remains enabled at
115200 baud.

## Web interface

The existing port-80 server now serves its own small, gzip-compressed JavaScript
and CSS from application flash. There are no CDN, font or framework downloads.
The standard ESPHome /events stream and /metrics endpoint are retained.

The page provides temperature, humidity and pressure first; clear live/unavailable
status; sample age; a browser-only Fahrenheit/Celsius preference; dew point and
altitude; and expandable Wi-Fi, uptime, heap and scan diagnostics. It links to
Grafana history and the existing metrics endpoint. Device titles come from the
same-origin event stream and are inserted as text.

Disconnected, incomplete, nonfinite and stale frames hide climate readings rather
than presenting old values as current. Each climate channel must have recent data,
the firmware freshness flag must be true, and sample age must remain below 15s.
The page reconnects using native EventSource behavior. Browser log streaming is
disabled; event logging is provided through syslog.

Layout and controls were checked on the office pilot at normal desktop size and
375px phone width. There was no horizontal overflow. Temperature switching and
diagnostic disclosure worked against actual device data. Light/dark styling uses
the browser's preference. No device configuration controls or HTTP OTA were added.

## Build and verification

All dependencies, compilation and automated tests ran in Docker. The office pilot
passed before the other four were updated serially over authenticated native OTA.
Each passed successful-boot, increasing-uptime, fresh-climate and heap checks.

Office linked application flash: **407,489 / 1,044,464 bytes (39.0%)**, up from
388,965 bytes. Static RAM: **34,552 / 81,920 bytes (42.2%)**, up from 33,388 bytes.
The combined syslog/time/UI change therefore adds **18,524 bytes flash** and
**1,164 bytes static RAM**. Runtime heap also depends on networking and browser
connections; the static figures do not account for every runtime allocation.

47 Python tests, 9 JavaScript state tests, and the existing C++ sensor, roaming,
OTA and startup-delay fault suites passed. Generated C++ checks verify the actual
UDP address table, final destination port, INFO syslog constructor, time binding,
send-only transport and disabled browser log streaming. Each live device's served
JS/CSS was decompressed and hashed against its source, fresh metrics and scrape
health were checked, and received syslog events were recorded.

Evidence: artifacts/observability-tests.log,
artifacts/observability-runtime-validation.json,
artifacts/sensor-ui-verified.jpg and artifacts/sensor-ui-mobile.jpg.

## Source and recovery

Shared template: sensor-office.yaml. Embedded assets: web/sensor-ui.mjs and
web/sensor-ui.css. Preparation and compiled checks: configure_observability.py.
Docker launcher: run-observability-upgrade.sh. Runtime validation:
verify_observability_runtime.py. The fleet builder copies the shared web assets
and checks the compiled logging configuration for future migrations.

Persistent release directory: **/ssd/docker/sensor-observability-20261024**.
Each sensor has previous.yaml, previous.bin, the new firmware.bin, build hash/log,
upload receipts and runtime observations. OTA passwords remain unchanged in the
existing private firmware directories. Restoring a previous application would use
authenticated native OTA with that device's existing key; no rollback was performed.
The reserved LittleFS area and hardware/sampling settings were preserved.

Prometheus, Grafana, historical samples and retention settings were unchanged by
this release.
