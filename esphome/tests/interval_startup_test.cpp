#include <cassert>
#include <cstdio>
#include "esphome/components/interval/interval.h"
#include "interval_runtime.inc"

int main() {
  using namespace esphome;
  constexpr uint64_t DAY = 86400000;
  for (uint64_t boot : {uint64_t(0), uint64_t(UINT32_MAX) - 2000}) {
    for (uint32_t seed : {uint32_t(0), uint32_t(0x80000000), uint32_t(UINT32_MAX)}) {
      test_random = seed;
      test_now = boot;
      interval::IntervalTrigger broken;
      broken.set_update_interval(DAY);
      broken.call_setup();
      broken.advance(boot + 4999);
      assert(broken.fired == 1);  // Reproduces the released boot-loop cause.

      test_now = boot;
      interval::IntervalTrigger fixed;
      fixed.set_update_interval(DAY);
      fixed.set_startup_delay(DAY);
      fixed.call_setup();
      fixed.advance(boot + 60000);
      assert(fixed.fired == 0);  // Safe-mode success window remains uninterrupted.
      fixed.advance(boot + DAY - 1);
      assert(fixed.fired == 0);
      const uint32_t offset = interval_offset(DAY);
      assert(offset <= 4999);
      fixed.advance(boot + DAY + offset);
      assert(fixed.fired == 1);
      fixed.advance(boot + 2 * DAY + offset - 1);
      assert(fixed.fired == 1);
      fixed.advance(boot + 2 * DAY + offset);
      assert(fixed.fired == 2);
    }
  }
  std::puts("PASS: released early restart reproduced; 24h startup delay blocks it, preserves daily cadence and uptime rollover");
}
