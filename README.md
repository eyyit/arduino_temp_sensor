# Climate sensor firmware

ESPHome firmware for the office, bedroom, living room, outside and kid sensors.
This replaces the former Arduino/Graphite firmware.

See [esphome/README.md](esphome/README.md) for configurations, Docker-only tests,
manual compilation and private provisioning requirements.

- [Device configurations and firmware](esphome/)
- [Custom sensor and Wi-Fi components](esphome/components/)
- [Local web interface](esphome/web/)
- [Firmware and UI tests](esphome/tests/)
- [Syslog and web UI release notes](esphome/SYSLOG-WEB-UI.md)

The firmware exposes Prometheus metrics and sends event logs to 10.0.0.2:514/UDP.
Build/update orchestration and history migration remain outside this repository.
Credentials, compiled binaries and private deployment records must not be committed.

## Legacy recovery reference

The former Arduino sketches and compile script are preserved in Git history at
commit `bd532b4`. Removing those source files does not change any installed device
or delete the separately archived recovery binaries. Historical engineering notes
under esphome/ retain references to those external archives.
