#include "office_roaming_policy.h"
#include "office_scan_watchdog.h"
#include <cassert>
#include <cstdio>
#include <limits>
using esphome::wifi::OfficeRoamingPolicy;
int main() {
  using esphome::wifi::OfficeScanWatchdog;
  OfficeScanWatchdog s;
  assert(!s.completed(s.generation)); // unsolicited or duplicate callback
  s.started(100, true);
  const auto original_generation = s.generation;
  assert(!s.started(200, false)); // reconnect must not replace an ongoing roam
  assert(s.since == 100 && s.limit == 5000 && s.generation == original_generation);
  assert(!s.expired(5099));
  assert(s.expired(5100)); // no callback at the roaming deadline
  s.resetting(5100);
  assert(!s.started(5200, false)); // no scan during radio recovery either
  assert(!s.completed(s.generation)); // delayed callback while radio is off
  assert(!s.reset_ready(6099));
  assert(s.reset_ready(6100));
  s.reset_complete();
  assert(!s.completed(s.generation)); // delayed callback before a new request
  const auto old_generation = s.generation - 1;
  s.started(6100, false); // reconnect must have its own bounded watchdog
  assert(!s.completed(old_generation)); // OLD callback after NEW scan starts
  assert(s.state == OfficeScanWatchdog::State::SCANNING);
  assert(s.since == 6100 && s.limit == 15000); // new deadline untouched
  assert(!s.expired(21099));
  assert(s.expired(21100));
  s.resetting(21100);
  assert(s.reset_ready(22100));
  s.reset_complete();
  s.started(22100, false);
  assert(s.completed(s.generation));
  assert(!s.expired(999999));
  assert(!s.completed(s.generation));
  // Repeated missing callbacks recover without depending on a future callback.
  for (unsigned i = 0; i < 20; ++i) {
    unsigned now = 100000 + i * 20000;
    s.started(now, true);
    assert(s.expired(now + 5000));
    s.resetting(now + 5000);
    assert(!s.completed(s.generation));
    assert(s.reset_ready(now + 6000));
    s.reset_complete();
  }
  unsigned rollover = std::numeric_limits<uint32_t>::max() - 100;
  s.started(rollover, true);
  assert(!s.expired(rollover + 4999));
  assert(s.expired(rollover + 5000));
  s.resetting(rollover);
  assert(s.reset_ready(rollover + 1000));
  OfficeScanWatchdog epochs;
  for (unsigned i = 0; i < OfficeScanWatchdog::CALLBACK_GENERATIONS; ++i) {
    assert(epochs.can_scan());
    epochs.started(i * 20000, true);
    const auto retired = epochs.generation;
    epochs.resetting(i * 20000 + 5000);
    const auto replacement = epochs.generation;
    epochs.resetting(i * 20000 + 5500); // OFF retries must not consume IDs
    assert(epochs.generation == replacement);
    epochs.reset_complete();
    if (epochs.can_scan()) {
      epochs.started(i * 20000 + 6000, false);
      // Every older generation remains invalid, even after many recoveries.
      for (unsigned old = 0; old <= retired; ++old)
        assert(!epochs.completed(old));
      assert(epochs.state == OfficeScanWatchdog::State::SCANNING);
      assert(epochs.completed(replacement));
    }
  }
  assert(!epochs.can_scan());
  const auto exhausted = epochs.generation;
  for (unsigned i = 0; i < 100; ++i) {
    epochs.resetting(i);
    epochs.reset_complete();
    assert(epochs.generation == exhausted); // never wrap/reuse old callbacks
    assert(!epochs.can_scan());
    assert(!epochs.completed(0));
  }
  OfficeRoamingPolicy p;
  assert(!p.ready(1000000));
  p.observe(0, true, -75);
  assert(!p.ready(119999));
  assert(p.ready(120000));
  p.started(120000);
  assert(!p.ready(419999));
  assert(p.ready(420000));
  // There is no three-scan limit. Cooldown applies even across disconnections.
  for (unsigned i = 0; i < 20; ++i) {
    unsigned now = 420000 + i * 300000;
    assert(p.ready(now));
    p.started(now);
  }
  p.observe(7000000, true, -74);
  assert(!p.ready(7000000));
  p.observe(7000001, true, -80);
  assert(!p.ready(7060000));
  assert(p.ready(7060001));
  p.observe(7060002, false, 0);
  assert(!p.ready(9000000));
  p.observe(9000000, true, -80);
  assert(!p.ready(9119999));
  assert(p.ready(9120000));
  // Millisecond wrap must not bypass or permanently block timing gates.
  OfficeRoamingPolicy w;
  unsigned start = std::numeric_limits<uint32_t>::max() - 100;
  w.observe(start, true, -80);
  assert(!w.ready(start + 119999));
  assert(w.ready(start + 120000));
  w.started(start + 120000);
  assert(!w.ready(start + 419999));
  assert(w.ready(start + 420000));
  auto candidate = OfficeRoamingPolicy::candidate;
  assert(candidate(-90, -78, true, false, true));
  assert(candidate(-75, -63, true, false, true));
  assert(!candidate(-74, -50, true, false, true));
  assert(!candidate(-91, -79, true, false, true));
  assert(!candidate(-80, -69, true, false, true));
  assert(!candidate(-80, -60, false, false, true));
  assert(!candidate(-80, -60, true, true, true));
  assert(!candidate(-80, -60, true, false, false));
  puts("PASS: timing, cooldown, continued scans, recovery, disconnect, wrap, candidate/security thresholds");
  puts("PASS: missing/late/duplicate callbacks, reconnect timeout, repeated radio recovery, watchdog rollover");
  puts("PASS: old callback after new scan, all retired generations, reset retries, exhaustion without ID reuse");
}
