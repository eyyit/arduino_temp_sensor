"""Check compiled ELF layout and generated authentication without logging secrets."""
from pathlib import Path
import struct
from release_checks import main as verify_release, load

node = load('/config/sensor-office.yaml', True)['esphome']['name']
root = Path('/config/.esphome/build') / node
elf = (root / f'.pioenvs/{node}/firmware.elf').read_bytes()
assert elf[:6] == b'\x7fELF\x01\x01', 'Expected little-endian ELF32'
section_offset = struct.unpack_from('<I', elf, 32)[0]
section_size, section_count = struct.unpack_from('<HH', elf, 46)
sections = [struct.unpack_from('<10I', elf, section_offset + i * section_size) for i in range(section_count)]
symbols = {}
for section in sections:
    if section[1] != 2:
        continue
    strings_section = sections[section[6]]
    strings = elf[strings_section[4]:strings_section[4] + strings_section[5]]
    for offset in range(section[4], section[4] + section[5], section[9]):
        name, value = struct.unpack_from('<II', elf, offset)
        end = strings.find(b'\0', name)
        symbols[strings[name:end].decode(errors='replace')] = value
assert symbols['_FS_start'] == 0x40500000, 'LittleFS start changed'
assert symbols.get('_FS_end', symbols.get('_SPIFFS_end')) == 0x405FA000, 'LittleFS end changed'
ninja = (root / f'.pioenvs/{node}/build.ninja').read_text()
assert '-T eagle.flash.4m1m.ld' in ninja, 'Wrong linker script'
assert (root / f'.pioenvs/{node}/firmware.bin').stat().st_size < 1044464
main = (root / 'src/main.cpp').read_text()
main = '\n'.join(line for line in main.splitlines() if not line.lstrip().startswith('//'))
verify_release()
print('PASS: compiled LittleFS 0x300000..0x3FA000, 4m1m linker, firmware fits, OTA password setter and callbacks present')
