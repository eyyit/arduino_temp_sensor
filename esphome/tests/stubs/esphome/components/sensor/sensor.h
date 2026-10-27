#pragma once
#include <cmath>
#include <functional>
#include <string>
namespace esphome::sensor {
class Sensor {
 public:
  float state{NAN};
  std::function<void(float)> callback;
  bool internal{false};
  std::string id{"test"}, name{"Test"}, unit{"C"};
  bool is_internal() const { return internal; }
  int get_accuracy_decimals() const { return 2; }
  const std::string &get_unit_of_measurement_ref() const { return unit; }
  void publish_state(float value) { state = value; if (callback) callback(value); }
};
}
