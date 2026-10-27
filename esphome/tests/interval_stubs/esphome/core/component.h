#pragma once
#include <cstdint>
#include <functional>
#include <utility>
namespace esphome {
inline uint64_t test_now = 0;
inline uint32_t test_random = 0;
inline uint32_t random_uint32() { return test_random; }
uint32_t interval_offset(uint32_t delay);
struct InternalSchedulerID { static constexpr uint32_t POLLING_UPDATE = 1; };
class PollingComponent {
 public:
  virtual ~PollingComponent() = default;
  virtual void setup() {}
  virtual void update() = 0;
  void call_setup();
  void start_poller();
  void stop_poller();
  uint32_t get_update_interval() const;
  void set_update_interval(uint32_t delay) { update_interval_ = delay; }
  void set_interval(uint32_t, uint32_t delay, std::function<void()> &&callback) {
    interval_ = std::move(callback); interval_at_ = test_now + interval_offset(delay);
    interval_active_ = true;
  }
  void cancel_interval(uint32_t) { interval_active_ = false; }
  void set_timeout(uint32_t delay, std::function<void()> callback) {
    timeout_ = std::move(callback); timeout_at_ = test_now + delay; timeout_active_ = true;
  }
  void advance(uint64_t until) {
    for (;;) {
      uint64_t next = UINT64_MAX;
      if (interval_active_) next = interval_at_;
      if (timeout_active_ && timeout_at_ < next) next = timeout_at_;
      if (next > until) break;
      test_now = next;
      if (timeout_active_ && timeout_at_ == next) {
        timeout_active_ = false; timeout_();
      } else {
        interval_at_ += update_interval_; interval_();
      }
    }
    test_now = until;
  }
 protected:
  uint32_t update_interval_ = 0;
  bool interval_active_ = false, timeout_active_ = false;
  uint64_t interval_at_ = 0, timeout_at_ = 0;
  std::function<void()> interval_, timeout_;
};
}
