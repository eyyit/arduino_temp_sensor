"""Extract unchanged interval scheduling bodies from the pinned runtime."""
from pathlib import Path
import re


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


core = Path('/esphome/esphome/core')
source = (core / 'scheduler.cpp').read_text()
constant = re.search(r'static constexpr uint32_t MAX_INTERVAL_DELAY = \d+;', source).group()
offset = function(source, 'uint32_t Scheduler::calculate_interval_offset_(')
offset = offset.replace('Scheduler::calculate_interval_offset_', 'interval_offset', 1)
source = (core / 'component.cpp').read_text()
methods = [function(source, signature) for signature in (
    'void PollingComponent::call_setup()', 'void PollingComponent::start_poller()',
    'void PollingComponent::stop_poller()', 'uint32_t PollingComponent::get_update_interval() const')]
Path('/config/tests/interval_runtime.inc').write_text(
    'namespace esphome {\n' + constant + '\n' + offset + '\n' + '\n'.join(methods) + '\n}\n')
