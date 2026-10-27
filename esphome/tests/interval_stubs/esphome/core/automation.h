#pragma once
namespace esphome {
template<typename... Args> class Trigger {
 public:
  unsigned fired = 0;
  void trigger() { ++fired; }
};
}
