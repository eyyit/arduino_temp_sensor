#pragma once
#include <cstdint>
#include <functional>
#include <utility>
namespace esphome {
class PollingComponent {
 public:
  virtual ~PollingComponent() = default;
  virtual void setup() {}
  virtual void update() {}
  virtual void loop() {}
  virtual void dump_config() {}
  bool warning{false}, failed{false};
  std::function<void()> pending;
  bool is_failed() const { return failed; }
  void reset_to_construction_state() { failed = false; }
  void status_set_warning() { warning = true; }
  void status_clear_warning() { warning = false; }
  void cancel_timeout(const char *) { pending = {}; }
  void set_timeout(const char *, uint32_t, std::function<void()> fn) { pending = std::move(fn); }
  void run_pending() { auto fn = std::move(pending); pending = {}; if (fn) fn(); }
};
}
