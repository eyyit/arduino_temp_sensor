# ESPHome climate sensor firmware

Current firmware source for office, bedroom, livingroom, outside and kid.
ESPHome replaces the former Arduino firmware. Legacy source is available in
Git history at commit bd532b4; it is no longer part of the current source tree.

## Source layout

- `sensor-office.yaml`: office configuration and shared hardware baseline.
- `sensor-{bedroom,livingroom,outside,kid}.yaml`: deployed device configurations.
- `components/`: BME280 recovery and Wi-Fi roaming/watchdog/OTA interlock.
- `web/`: local sensor page, embedded into application flash.
- `tests/`: firmware fault, release, layout and browser state tests.
- `SYSLOG-WEB-UI.md`: deployed syslog/UI behavior and release evidence.

Configs contain secret references only. Supply a private `secrets.yaml` using
`secrets.example.yaml` as the key schema, never its placeholder values.
Each device needs its own existing OTA key. Do not reuse one device's secrets
for another. The external release tooling also requires private AirScape
credential provenance at `.private/wifi-provenance.json` and fails closed when
it is absent. Neither file belongs in Git.

Hardware remains GPIO4 SDA / GPIO5 SCL, BME280 0x76, forced 1x sampling every
5 seconds, with 1 MB LittleFS reserved. Syslog goes to 10.0.0.2:514/UDP.

## Docker-only tests

From this directory, with the existing pinned image available:

```sh
docker run --rm --network none -v "$PWD:/config" --entrypoint sh \
  ghcr.io/esphome/esphome@sha256:1dff19a59070cdb523b76576cdc74d200f862d43c1407809088855920cf57145 \
  /config/tests/run-fault-tests.sh
docker run --rm --network none -v "$PWD:/config" -w /config \
  node:20-alpine node --test tests/test_sensor_ui.mjs
```

All dependencies and compilation must stay in Docker. No host package changes.
A manual compile uses the same ESPHome image with
`compile /config/sensor-office.yaml` (or the selected device YAML).
Compilation does not upload. Authenticated release checks and ELF layout checks
are retained in `tests/`; run them with the correct private release inputs.

## Deployment tooling stays separate

Fleet orchestration, automatic builds/updates, Graphite history import,
Prometheus/Grafana changes, credentials, binaries and upload receipts are
intentionally outside this repository. No automatic build or upload hook is
installed here. Existing tooling is still in the Windows Sensors workspace and
its server release directories; moving source does not change its configured
input paths. Future releases must explicitly synchronize reviewed source before
using that tooling.

Release archive: `/ssd/docker/sensor-observability-20261024/`.
The deployment notes describe the October 24 release, not a fresh runtime check.
