"""Release checks with deliberately secret-free diagnostics."""
import codecs
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import sys
import yaml

class ReleaseError(Exception):
    pass

def require(condition, message):
    if not condition:
        raise ReleaseError(message)

@dataclass(frozen=True)
class Secret:
    name: str

class ConfigLoader(yaml.SafeLoader):
    pass

ConfigLoader.add_constructor('!secret', lambda loader, node: Secret(loader.construct_scalar(node)))

def load(path, config=False):
    try:
        return yaml.load(Path(path).read_text(), Loader=ConfigLoader if config else yaml.SafeLoader)
    except Exception:
        raise ReleaseError('Cannot safely parse required configuration') from None

def wifi_digest(secrets):
    payload = {'ssid': secrets['wifi_ssid'], 'password': secrets['wifi_password']}
    return hashlib.sha256(json.dumps(payload, ensure_ascii=False, sort_keys=True).encode()).hexdigest()

def validate_credentials(secrets, provenance):
    require(isinstance(secrets, dict), 'Missing private secrets')
    for key in ('wifi_ssid', 'wifi_password', 'ota_password'):
        value = secrets.get(key)
        require(isinstance(value, str) and bool(value.strip()), 'Empty or missing credential')
        require(not re.search(r'compile_only|replace[_ -]|changeme|your[_ -]|placeholder|example', value, re.I),
                'Placeholder credential rejected')
    require(1 <= len(secrets['wifi_ssid'].encode()) <= 32, 'Invalid SSID length')
    password = secrets['wifi_password']
    require(8 <= len(password) <= 63 or bool(re.fullmatch(r'[a-fA-F0-9]{64}', password)), 'Invalid WiFi credential length')
    require(password.lower() not in {'password', '12345678', '123456789', 'changeme'}, 'Default WiFi password rejected')
    require(bool(re.fullmatch(r'[a-f0-9]{64}', secrets['ota_password'])) and
            len(set(secrets['ota_password'])) >= 8, 'OTA password must retain the generated random 256-bit format')
    require(secrets['ota_password'] != password, 'WiFi and OTA passwords must be distinct')
    require(isinstance(provenance, dict) and provenance.get('sha256') == wifi_digest(secrets),
            'WiFi does not match recorded AirScape provisioning source')

EXPECTED = {'on_begin': True, 'on_end': False, 'on_error': False}
def validate_config(config):
    intervals = config.get('interval', [])
    require(len(intervals) == 1 and intervals[0].get('interval') == '24h' and
            intervals[0].get('startup_delay') == '24h' and
            intervals[0].get('then') == [{'lambda': 'ESP.restart();'}],
            'Daily restart must wait a full 24h after boot')
    require(config.get('wifi', {}).get('ssid') == Secret('wifi_ssid') and
            config['wifi'].get('password') == Secret('wifi_password'), 'WiFi must reference private secrets')
    receivers = config.get('ota')
    require(isinstance(receivers, list) and len(receivers) == 1, 'Exactly one native OTA receiver required')
    receiver = receivers[0]
    require(receiver.get('platform') == 'esphome' and receiver.get('password') == Secret('ota_password'),
            'Native OTA must use the private OTA password')
    require(config.get('web_server', {}).get('ota') is False, 'HTTP OTA must remain disabled')
    for event, busy in EXPECTED.items():
        actions = receiver.get(event, {}).get('then')
        wanted = f'esphome::wifi::global_wifi_component->office_set_ota_busy({str(busy).lower()});'
        require(isinstance(actions, list) and len(actions) == 1 and
                set(actions[0]) == {'lambda'} and ''.join(actions[0]['lambda'].split()) == wanted,
                'Incorrect OTA begin/end/error event wiring')

def decode_literal(literal):
    try:
        return codecs.escape_decode(literal[1:-1].encode('utf8'))[0].decode('utf8')
    except Exception:
        raise ReleaseError('Cannot validate generated credential literal') from None

LITERAL = r'"(?:[^"\\]|\\.)*"'
def validate_generated(source, secrets):
    # Comments contain configuration previews and must never satisfy a check.
    source = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
    source = '\n'.join(line for line in source.splitlines() if not line.lstrip().startswith('//'))
    timers = re.findall(r'new\((\w+)\)\s+interval::IntervalTrigger\(\);', source)
    require(len(timers) == 1, 'Exactly one compiled daily restart timer required')
    timer = re.escape(timers[0])
    require(re.search(timer + r'->set_update_interval\(86400000\);', source) is not None and
            re.search(timer + r'->set_startup_delay\(86400000\);', source) is not None,
            'Compiled daily restart startup delay is missing or incorrect')
    ssids = re.findall(r'(\w+)\.set_ssid\((' + LITERAL + r')\);', source)
    require(len(ssids) == 1 and decode_literal(ssids[0][1]) == secrets['wifi_ssid'], 'Compiled SSID mismatch')
    ap = ssids[0][0]
    passwords = re.findall(re.escape(ap) + r'\.set_password\((' + LITERAL + r')\);', source)
    require(len(passwords) == 1 and decode_literal(passwords[0]) == secrets['wifi_password'], 'Compiled WiFi password mismatch')
    auth = re.findall(r'(\w+)->set_auth_password\((' + LITERAL + r')\);', source)
    require(len(auth) == 1 and decode_literal(auth[0][1]) == secrets['ota_password'], 'Compiled OTA password mismatch')
    receiver = auth[0][0]
    require('ESPHomeOTAComponent' in source, 'Missing native receiver')
    triggers = re.findall(r'new\((\w+)\)\s+ota::OTA(Start|End|Error)Trigger\((\w+)\);', source)
    require(len(triggers) == 3 and {kind for _, kind, _ in triggers} == {'Start', 'End', 'Error'},
            'Missing or duplicate compiled OTA event trigger')
    for trigger, kind, parent in triggers:
        require(parent == receiver, 'OTA trigger attached to the wrong receiver')
        automations = re.findall(r'new\((\w+)\)\s+Automation<[^>]*>\(' + re.escape(trigger) + r'\);', source)
        require(len(automations) == 1, 'Missing event automation binding')
        actions = re.findall(re.escape(automations[0]) + r'->add_actions\(\{(\w+)\}\);', source)
        require(len(actions) == 1, 'Missing event action binding')
        pattern = r'new\(' + re.escape(actions[0]) + r'\)\s+(?:Stateless)?LambdaAction<[^>]*>\('
        match = re.search(pattern, source)
        require(match is not None, 'Missing compiled OTA lambda')
        opening = source.find('{', match.end())
        require(opening >= 0, 'Malformed generated OTA lambda')
        depth, end = 1, opening + 1
        while depth and end < len(source):
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        body = source[opening:end]
        calls = re.findall(r'office_set_ota_busy\((true|false)\)', body)
        require(calls == [('true' if kind == 'Start' else 'false')], 'Wrong compiled OTA event action')

def main():
    root = Path('/config')
    config = load(root / 'sensor-office.yaml', True)
    secrets = load(root / 'secrets.yaml')
    try:
        provenance = json.loads((root / '.private/wifi-provenance.json').read_text())
    except Exception:
        raise ReleaseError('Missing private provisioning provenance') from None
    validate_credentials(secrets, provenance)
    validate_config(config)
    if '--preflight' not in sys.argv:
        node = config['esphome']['name']
        require(re.fullmatch(r'sensor_[a-z][a-z0-9_]*', node) is not None, 'Invalid release node name')
        validate_generated((root / f'.esphome/build/{node}/src/main.cpp').read_text(), secrets)
    print('PASS: non-placeholder private credentials, AirScape provenance, authenticated OTA and exact event wiring' +
          (' (configuration)' if '--preflight' in sys.argv else ' (configuration and generated C++)'))

if __name__ == '__main__':
    try:
        main()
    except ReleaseError as error:
        print('RELEASE CHECK FAILED: ' + str(error), file=sys.stderr)
        sys.exit(1)
