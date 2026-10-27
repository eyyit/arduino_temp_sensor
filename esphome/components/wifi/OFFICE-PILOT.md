# Local Wi-Fi override

Vendored from the official ESPHome 2026.10.0 Docker image pinned in
../../README.md.
This is an ESP8266-only climate sensor override, not a general-purpose ESPHome
Wi-Fi replacement. Do not use with another platform or ESPHome release without
review. Upstream source is licensed GPL-3.0; see UPSTREAM-LICENSE.

Local modifications are in wifi_component.h, wifi_component.cpp,
wifi_component_esp8266.cpp, and the added office_roaming_policy.h.
The original component is retained in artifacts/upstream-wifi.tar for comparison.

Uses the native ESPHome asynchronous scan/callback/state machine. No parallel
Arduino scan engine is introduced. Standard initial three-check scheduling is
replaced by continuous weak-link observation with AirScape thresholds. Manual
force-roam also respects these thresholds. Diagnostic counters are per boot.
Candidate storage is capped at 16 entries, as in AirScape. WPA2 or mixed WPA/WPA2
auth modes are eligible; WPA-only, WEP and open APs are excluded from roam scans.
The current SSID/BSSID and RSSI are checked after the scan; connection retries
use normal ESPHome scan-based fallback rather than a persistent BSSID pin.

ESP8266 has no supported SDK scan-stop operation. A missing roaming callback at
five seconds now triggers a Wi-Fi adapter reset. Initial/reconnect scans have
a fifteen-second watchdog, including scan-start failures. Recovery turns station
and AP modes OFF, discards scan state/results, holds OFF for at least one second
using the main loop (no blocking sleep), then starts normal scan/reconnection.
If OFF fails, retry OFF at one-second intervals without starting another scan.
Callbacks without an outstanding request or during reset are ignored. A previous
version only checked SCANNING state, so a pre-reset callback arriving after the
next scan started could be accepted. Each recovery generation now gets a distinct
template-generated SDK callback function; the function's compile-time generation
must match the current generation before any flags/results/counters are touched.
The generation is retired before radio OFF and never recycled. An old callback
therefore cannot finish a new-generation scan, including after multiple resets.
The generation changes once per recovery, not once per OFF retry.

There are 64 callback generations per boot. After exhausting them the pilot stops
all scanning and uses the configured SSID/password for unpinned connection retries.
Background roaming is disabled until reboot rather than reusing an old callback
identity. The existing daily restart is unchanged. This is a bounded safety limit,
not a claim of unlimited recovery under a permanently defective driver. The SDK
supplies no request ID, so checking a mutable generation inside one shared
callback would not solve the race. Distinct entry points preserve identity when
the SDK invokes the callback registered for that scan. Actual SDK/RF behavior
still requires hardware tests; the closed SDK's internal routing is not modeled
by host tests.
This recovers without waiting for the missing callback or rebooting the MCU.
It briefly interrupts Wi-Fi and metrics access. Sensor pins/settings are intact.
The existing daily reboot and ESPHome's separate connection-loss reboot timeout
are unchanged; they are not the mechanism used to recover the stuck scan.
WiFi Scan Recoveries counts adapter recovery episodes per boot. Existing
five-minute roaming scan cooldown is preserved after recovery.

Native password-protected OTA is configured in each device YAML.
The office_ota_interlock.h integration suppresses new scans and defers radio
recovery during OTA; completed results cannot initiate roaming while it is busy. This is not
AirScape's task-priority implementation: ESP8266 ESPHome uses a cooperative main
loop, and RF activity can interrupt metrics delivery. No RF or sensor acquisition
latency acceptance is claimed from compilation or host policy tests.
