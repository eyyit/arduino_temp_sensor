#pragma once
#include <cstdint>

namespace esphome::wifi {
// Shared by normal reconnect and background roaming scans.
struct OfficeScanWatchdog {
  enum class State { IDLE, SCANNING, RESETTING };
  State state{State::IDLE};
  uint32_t since{0}, limit{0};
  // Each radio-reset generation gets a distinct SDK function pointer. Never
  // recycle one: a callback can arrive arbitrarily late with no SDK request ID.
  static constexpr uint32_t CALLBACK_GENERATIONS = 64;
  uint32_t generation{0};
  bool can_scan() const { return generation < CALLBACK_GENERATIONS; }
  bool started(uint32_t now, bool roaming) {
    if (state != State::IDLE || !can_scan()) return false;
    state = State::SCANNING;
    since = now;
    limit = roaming ? 5000 : 15000;
    return true;
  }
  bool expired(uint32_t now) const {
    return state == State::SCANNING && uint32_t(now - since) >= limit;
  }
  // Reject callbacks when no scan is outstanding or the radio is being reset.
  bool completed(uint32_t callback_generation) {
    if (callback_generation != generation || state != State::SCANNING) return false;
    state = State::IDLE;
    return true;
  }
  void resetting(uint32_t now) {
    if (state != State::RESETTING && can_scan()) ++generation;
    state = State::RESETTING;
    since = now;
  }
  bool reset_ready(uint32_t now) const {
    return state == State::RESETTING && uint32_t(now - since) >= 1000;
  }
  void reset_complete() { state = State::IDLE; }
};
}  // namespace esphome::wifi
