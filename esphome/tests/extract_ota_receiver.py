"""Extract pinned upstream receiver methods unchanged for a host fault harness."""
from pathlib import Path
import re
source = Path('/esphome/esphome/components/esphome/ota/ota_esphome.cpp').read_text()
header = Path('/esphome/esphome/components/esphome/ota/ota_esphome.h').read_text()
assert re.search(r'CLIENT_FEATURE_SUPPORTS_SHA256_AUTH\s*=\s*0x02\s*;', source)
assert re.search(r'OTA_BUFFER_SIZE\s*=\s*1040\s*;', header)
assert re.search(r'OTA_SOCKET_TIMEOUT_DATA\s*=\s*105000\s*;', source)
names = ['handle_data_', 'readall_', 'writeall_', 'client_left_before_start_',
         'handle_read_error_', 'read_size_', 'write_flash_', 'receive_data_',
         'send_chunk_acks_', 'cleanup_connection_', 'select_auth_type_',
         'handle_auth_read_', 'cleanup_auth_']
out = []
for name in names:
    marker = 'ESPHomeOTAComponent::' + name + '('
    at = source.index(marker)
    start = source.rfind('\n', 0, at) + 1
    opening = source.index('{', at)
    # These methods have balanced braces in literals/comments as well as code.
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    out.append(source[start:end])
Path('/config/tests/ota_receiver_methods.inc').write_text('\n\n'.join(out))
print('Extracted 13 pinned OTA receiver methods without modifying their bodies')
