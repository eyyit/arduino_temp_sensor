#include "bme280_base.h"
#include "esphome/core/hal.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
using namespace esphome;
using namespace esphome::bme280_base;

class FakeBME : public BME280Component {
 public:
  std::array<uint8_t, 256> regs{};
  sensor::Sensor t, p, h, fahrenheit, altitude, dew;
  bool present{true};
  int bad_read{-1}, bad_write{-1};
  unsigned resets{0};
  FakeBME() {
    set_temperature_sensor(&t); set_pressure_sensor(&p); set_humidity_sensor(&h);
    set_temperature_oversampling(BME280_OVERSAMPLING_1X);
    set_pressure_oversampling(BME280_OVERSAMPLING_1X);
    set_humidity_oversampling(BME280_OVERSAMPLING_1X);
    regs[0xd0] = 0x60;
    put16(0x88, 27504); put16(0x8a, 26435); put16(0x8c, -1000);
    put16(0x8e, 36477); put16(0x90, -10685); put16(0x92, 3024);
    put16(0x94, 2855); put16(0x96, 140); put16(0x98, -7);
    put16(0x9a, 15500); put16(0x9c, -14600); put16(0x9e, 6000);
    regs[0xa1] = 75; put16(0xe1, 362); regs[0xe3] = 0;
    regs[0xe4] = 20; regs[0xe5] = 0x25; regs[0xe6] = 3; regs[0xe7] = 30;
    raw(0xf7, 415148); raw(0xfa, 519888); regs[0xfd] = 30000 >> 8; regs[0xfe] = 30000 & 255;
    h.callback = [this](float rh) {
      fahrenheit.publish_state(t.state * 1.8f + 32);
      altitude.publish_state(44330 * (1 - powf(p.state / 1020, 0.1903f)));
      if (!std::isfinite(t.state) || !std::isfinite(rh)) { dew.publish_state(NAN); return; }
      float safe = fminf(100, fmaxf(0.1f, rh));
      float alpha = 17.27f * t.state / (237.7f + t.state) + logf(safe / 100);
      dew.publish_state(237.7f * alpha / (17.27f - alpha));
    };
  }
  bool initialized() const { return initialized_; }
  void put16(int a, int value) { regs[a] = value & 255; regs[a+1] = (value >> 8) & 255; }
  void raw(int a, int value) { regs[a] = value >> 12; regs[a+1] = (value >> 4) & 255; regs[a+2] = (value & 15) << 4; }
  bool read_byte(uint8_t a, uint8_t *out) override {
    if (!present || a == bad_read) return false;
    *out = regs[a]; return true;
  }
  bool read_byte_16(uint8_t a, uint16_t *out) override {
    if (!present || a == bad_read) return false;
    *out = (regs[a] << 8) | regs[a+1]; return true;
  }
  bool write_byte(uint8_t a, uint8_t value) override {
    if (!present || a == bad_write) return false;
    if (a == 0xe0) { ++resets; regs[0xf2] = regs[0xf4] = 0; }
    regs[a] = value; return true;
  }
  bool read_bytes(uint8_t a, uint8_t *out, size_t len) override {
    if (!present || a == bad_read) return false;
    std::memcpy(out, regs.data() + a, len);
    // Following sensor power loss, humidity oversampling resets to disabled.
    if (a == 0xf7 && len == 8 && (regs[0xf2] & 7) == 0) { out[6] = 0x80; out[7] = 0; }
    return true;
  }
  void good() { update(); run_pending(); assert(sample_fresh()); assert(!warning); }
  void invalid() {
    assert(!sample_fresh()); assert(warning); assert(!failed);
    for (auto *entity : {&t, &p, &h, &fahrenheit, &altitude, &dew}) assert(std::isnan(entity->state));
  }
};

// Compile the pinned upstream exporter row unchanged, using only stream/entity
// test doubles. The real sensor driver above supplies the tested states.
constexpr int VALUE_ACCURACY_MAX_LEN = 64;
#define ESPHOME_F(x) x
inline void value_accuracy_to_buf(char *buf, float value, int decimals) {
  std::snprintf(buf, VALUE_ACCURACY_MAX_LEN, "%.*f", decimals, double(value));
}
struct AsyncResponseStream {
  std::string output;
  void print(const char *s) { output += s; }
  void print(const std::string &s) { output += s; }
};
class PrometheusHandler {
 public:
  bool include_internal_{false};
  std::string relabel_id_(sensor::Sensor *s) { return s->id; }
  std::string relabel_name_(sensor::Sensor *s) { return s->name; }
  void add_area_label_(AsyncResponseStream *, std::string &) {}
  void add_node_label_(AsyncResponseStream *, std::string &) {}
  void add_friendly_name_label_(AsyncResponseStream *, std::string &) {}
  void sensor_row_(AsyncResponseStream *, sensor::Sensor *, std::string &, std::string &, std::string &);
};
#include "prometheus_sensor_row.inc"
std::string metrics(sensor::Sensor &sensor) {
  PrometheusHandler handler; AsyncResponseStream stream; std::string blank;
  handler.sensor_row_(&stream, &sensor, blank, blank, blank); return stream.output;
}
int main() {
  FakeBME boot; boot.present = false; boot.setup(); boot.invalid();
  assert(std::isnan(boot.sample_age_seconds()));
  boot.present = true; boot.good(); assert(boot.resets == 1);
  assert(metrics(boot.t).find("esphome_sensor_value") != std::string::npos);
  sensor::Sensor precision; precision.state = 12.3456f;
  assert(metrics(precision).find("12.35\n") != std::string::npos);
  boot.present = false; boot.update(); boot.invalid();
  for (auto *s : {&boot.t, &boot.p, &boot.h, &boot.fahrenheit, &boot.altitude, &boot.dew}) {
    auto text = metrics(*s);
    assert(text.find("esphome_sensor_value") == std::string::npos);
    assert(text.find("} 1\n") != std::string::npos);
  }
  boot.present = true; boot.good(); assert(boot.resets == 2);
  // Every calibration transaction must fail setup, not become silent zero data.
  for (int address : {0x88,0x8a,0x8c,0x8e,0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e,
                      0xa1,0xe1,0xe3,0xe4,0xe5,0xe6,0xe7}) {
    FakeBME f; f.bad_read = address; f.setup(); f.invalid(); assert(!f.initialized());
    f.bad_read = -1; f.good();
  }
  for (int address : {0xd0,0xf3,0xf2,0xf5}) {
    FakeBME f; f.bad_read = address; f.setup(); f.invalid(); f.bad_read = -1; f.good();
  }
  for (int address : {0xe0,0xf2,0xf5,0xf4}) {
    FakeBME f; f.bad_write = address; f.update(); f.invalid(); f.bad_write = -1; f.good();
  }
  FakeBME read; read.good(); read.bad_read = 0xf7; read.update(); read.run_pending(); read.invalid();
  read.bad_read = -1; read.good();
  FakeBME chip; chip.regs[0xd0] = 0x58; chip.setup(); chip.invalid();
  chip.regs[0xd0] = 0x60; chip.good();
  FakeBME zero; zero.put16(0x8e, 0); zero.setup(); zero.invalid();
  FakeBME power; power.good(); power.regs[0xf2] = 0;
  power.update(); power.run_pending(); power.invalid();
  power.good(); assert((power.regs[0xf2] & 7) == 1); // full setup restores humidity
  FakeBME nvm; nvm.regs[0xf3] = 1; nvm.setup(); nvm.invalid();
  nvm.regs[0xf3] = 0; nvm.good();
  for (int channel : {0xf7, 0xfa, 0xfd}) {
    FakeBME f; f.good();
    if (channel == 0xfd) { f.regs[0xfd] = 0x80; f.regs[0xfe] = 0; }
    else f.raw(channel, 0x80000);
    f.update(); f.run_pending(); f.invalid();
  }
  FakeBME busy; busy.good(); busy.update(); busy.regs[0xf3] = 8; busy.run_pending(); busy.invalid();
  busy.regs[0xf3] = 0; busy.good();
  FakeBME stale; stale.good(); auto good_at = fake_millis;
  fake_millis = good_at + 14999; stale.loop(); assert(stale.sample_fresh());
  fake_millis = good_at + 15000; stale.loop(); stale.invalid(); stale.good();
  stale.update(); assert(stale.pending);
  fake_millis += 15000; stale.loop(); stale.invalid(); assert(!stale.pending);
  stale.run_pending(); stale.invalid(); // cancelled conversion cannot resurrect values
  stale.good();
  FakeBME wrap; fake_millis = std::numeric_limits<uint32_t>::max() - 100; wrap.good();
  good_at = fake_millis; fake_millis = good_at + 15000; wrap.loop(); wrap.invalid(); wrap.good();
  puts("PASS: actual BME280 driver: absent boot, recovery, each calibration/setup fault, conversion/read failures,");
  puts("      invalid channels, busy conversion, stale cutoff/rollover, derived invalidation, exporter omission/2 decimals");
}
