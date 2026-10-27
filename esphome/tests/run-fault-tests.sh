#!/bin/sh
set -eu
cd /config
python3 tests/release_checks_test.py
python3 tests/extract_interval_runtime.py
g++ -std=c++17 -Wall -Wextra -Werror -Itests/interval_stubs -I/esphome -Itests \
  tests/interval_startup_test.cpp -o /config/interval-startup-test
/config/interval-startup-test
g++ -std=c++17 -Wall -Wextra -Werror -Icomponents/wifi tests/roaming_policy_test.cpp -o /config/roaming-policy-test
/config/roaming-policy-test
# Extract the exact pinned upstream sensor row for exercising its NaN behavior.
python3 - <<'PY'
from pathlib import Path
source = Path('/esphome/esphome/components/prometheus/prometheus_handler.cpp').read_text()
start = source.index('void PrometheusHandler::sensor_row_(')
opening = source.index('{', start)
depth = 1
end = opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
Path('/config/tests/prometheus_sensor_row.inc').write_text(source[start:end])
PY
g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter \
  -Itests -Itests/stubs -Icomponents/bme280_base \
  tests/sensor_fault_test.cpp components/bme280_base/bme280_base.cpp -o /config/sensor-fault-test
/config/sensor-fault-test
python3 tests/extract_ota_receiver.py
g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter \
  -Itests -Itests/stubs -Icomponents/wifi tests/ota_fault_test.cpp -o /config/ota-fault-test
/config/ota-fault-test
