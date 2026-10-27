import copy
import json
import unittest
from release_checks import (ReleaseError, Secret, validate_credentials, validate_config,
                            validate_generated, wifi_digest)

SECRETS = {'wifi_ssid': 'Fixture network', 'wifi_password': 'fixture-strong-pass!',
           'ota_password': '0123456789abcdef' * 4}

def config():
    receiver = {'platform': 'esphome', 'password': Secret('ota_password')}
    for event, busy in [('on_begin', 'true'), ('on_end', 'false'), ('on_error', 'false')]:
        receiver[event] = {'then': [{'lambda': 'esphome::wifi::global_wifi_component->office_set_ota_busy(' + busy + ');'}]}
    return {'wifi': {'ssid': Secret('wifi_ssid'), 'password': Secret('wifi_password')},
            'ota': [receiver], 'web_server': {'ota': False},
            'interval': [{'interval': '24h', 'startup_delay': '24h', 'then': [{'lambda': 'ESP.restart();'}]}]}

def generated(secrets=SECRETS):
    source = ('ESPHomeOTAComponent *receiver;\n'
              'ap.set_ssid(' + json.dumps(secrets['wifi_ssid'], ensure_ascii=False) + ');\n'
              'ap.set_password(' + json.dumps(secrets['wifi_password'], ensure_ascii=False) + ');\n'
              'receiver->set_auth_password(' + json.dumps(secrets['ota_password']) + ');\n')
    for event, busy, arg in [('Start','true',''), ('End','false',''), ('Error','false','uint8_t')]:
        source += f'new(t{event}) ota::OTA{event}Trigger(receiver);\n'
        source += f'new(a{event}) Automation<{arg}>(t{event});\n'
        source += f'new(l{event}) StatelessLambdaAction<{arg}>([]() -> void {{\n'
        source += f'esphome::wifi::global_wifi_component->office_set_ota_busy({busy});\n}});\n'
        source += f'a{event}->add_actions({{l{event}}});\n'
    return source + ('new(daily) interval::IntervalTrigger();\n'
                     'daily->set_update_interval(86400000);\n'
                     'daily->set_startup_delay(86400000);\n')

class Checks(unittest.TestCase):
    def test_valid(self):
        validate_credentials(SECRETS, {'sha256': wifi_digest(SECRETS)})
        validate_config(config())
        validate_generated(generated(), SECRETS)

    def test_placeholders_and_empty(self):
        for key in SECRETS:
            for value in ('', 'COMPILE_ONLY_NOT_A_REAL_PASSWORD', 'REPLACE_WITH_A_PASSWORD', 'changeme'):
                bad = dict(SECRETS, **{key:value})
                with self.subTest(key=key, case=value), self.assertRaises(ReleaseError):
                    validate_credentials(bad, {'sha256':wifi_digest(bad)})

    def test_weak_or_changed_credentials(self):
        for change in ({'wifi_password':'password'}, {'ota_password':'a'*64}, {'wifi_ssid':'x'*33}):
            bad = dict(SECRETS, **change)
            with self.assertRaises(ReleaseError): validate_credentials(bad, {'sha256':wifi_digest(bad)})
        with self.assertRaises(ReleaseError): validate_credentials(SECRETS, {'sha256':'wrong'})

    def test_yaml_events(self):
        for event in ('on_begin','on_end','on_error'):
            bad = config(); del bad['ota'][0][event]
            with self.assertRaises(ReleaseError): validate_config(bad)
            bad = config(); bad['ota'][0][event]['then'][0]['lambda'] = 'office_set_ota_busy(true);'
            with self.assertRaises(ReleaseError): validate_config(bad)
        bad = config(); bad['web_server']['ota'] = True
        with self.assertRaises(ReleaseError): validate_config(bad)
        bad = config(); bad['ota'][0]['password'] = 'inline-secret'
        with self.assertRaises(ReleaseError): validate_config(bad)

    def test_compiled_wiring_mutations(self):
        source = generated()
        for before, after in [
            ('OTAStartTrigger(receiver)', 'OTAStartTrigger(other_receiver)'),
            ('Automation<>(tStart)', 'Automation<>(tEnd)'),
            ('aStart->add_actions({lStart})', 'aStart->add_actions({lEnd})'),
            ('office_set_ota_busy(true)', 'office_set_ota_busy(false)'),
            ('OTAErrorTrigger(receiver)', 'OTAEndTrigger(receiver)'),
            ('set_auth_password(', 'set_password('),
        ]:
            with self.subTest(case=before), self.assertRaises(ReleaseError):
                validate_generated(source.replace(before, after), SECRETS)
        with self.assertRaises(ReleaseError):
            validate_generated('\n'.join('// '+line for line in source.splitlines()), SECRETS)

    def test_compiled_credential_mismatch(self):
        for key in SECRETS:
            bad = dict(SECRETS, **{key:SECRETS[key]+'x'})
            with self.assertRaises(ReleaseError): validate_generated(generated(bad), SECRETS)

    def test_cpp_literal_escaping(self):
        special = dict(SECRETS, wifi_ssid='Office "x" \\ café', wifi_password='fixture"quoted\\password')
        validate_generated(generated(special), special)

    def test_daily_restart_delay(self):
        for delay in (None, '0s', '60s', '12h'):
            bad = config()
            if delay is None:
                del bad['interval'][0]['startup_delay']
            else:
                bad['interval'][0]['startup_delay'] = delay
            with self.subTest(delay=delay), self.assertRaises(ReleaseError):
                validate_config(bad)
        for replacement in ('', 'daily->set_startup_delay(0);', 'other->set_startup_delay(86400000);'):
            with self.subTest(replacement=replacement), self.assertRaises(ReleaseError):
                validate_generated(generated().replace('daily->set_startup_delay(86400000);', replacement), SECRETS)

if __name__ == '__main__':
    unittest.main(verbosity=1)
