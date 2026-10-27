#pragma once
#include <cstdint>

namespace esphome::wifi {
// AirScape thresholds; wrap-safe millisecond arithmetic. No attempt-count limit.
struct OfficeRoamingPolicy {
  uint32_t connected_at{0}, weak_at{0}, scan_at{0};
  bool connected{false}, weak{false}, scanned{false};
  void observe(uint32_t now, bool online, int rssi) {
    if (!online) {
      connected = weak = false;
      return;
    }
    if (!connected) {
      connected = true;
      connected_at = now;
    }
    if (rssi > -75) {
      weak = false;
    } else if (!weak) {
      weak = true;
      weak_at = now;
    }
  }
  bool ready(uint32_t now) const {
    return connected && weak && uint32_t(now - connected_at) >= 120000 &&
           uint32_t(now - weak_at) >= 60000 && (!scanned || uint32_t(now - scan_at) >= 300000);
  }
  void started(uint32_t now) { scanned = true; scan_at = now; }
  static bool candidate(int current, int other, bool same_ssid, bool same_bssid, bool wpa2) {
    return current <= -75 && same_ssid && !same_bssid && wpa2 && other >= -78 && other - current >= 12;
  }
};
}  // namespace esphome::wifi
