#pragma once
#include "office_scan_watchdog.h"
namespace esphome::wifi {
class OfficeOTAInterlock {
 public:
  void set_active(bool active) { active_ = active; }
  bool active() const { return active_; }
  bool may_scan(const OfficeScanWatchdog &scan) const {
    return !active_ && scan.state == OfficeScanWatchdog::State::IDLE && scan.can_scan();
  }
  bool may_recover_radio() const { return !active_; }
  bool may_use_scan_result() const { return !active_; }
 private:
  bool active_{false};
};
}
