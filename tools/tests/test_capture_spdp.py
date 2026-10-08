"""Check native RustDDS observation and preserve strict OpenDDS defaults."""
import contextlib
import io
from pathlib import Path
import runpy
import socket
import struct
import unittest
from unittest.mock import MagicMock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'capture_spdp.py'
LOCAL = '192.168.10.15'


def spdp(addresses):
    parameters = b''
    for address in addresses:
        parameters += struct.pack('<HHiI', 0x32, 24, 1, 7410)
        parameters += bytes(12) + socket.inet_aton(address)
    payload = b'\x00\x03\x00\x00' + parameters + struct.pack('<HH', 1, 0)
    body = struct.pack('<HH', 0, 16) + bytes(4) + b'\x00\x01\x00\xc2' + bytes(8) + payload
    return b'RTPS\x02\x03\x01\x12' + bytes(12) + struct.pack('<BBH', 0x15, 5, len(body)) + body


class ObservationTest(unittest.TestCase):
    def observe(self, native, ttl, addresses):
        receiver = MagicMock()
        receiver.__enter__.return_value = receiver
        ancillary = [(socket.IPPROTO_IP, socket.IP_TTL, struct.pack('i', ttl)),
                     (socket.IPPROTO_IP, 8, struct.pack('i', 3) + socket.inet_aton(LOCAL) + socket.inet_aton('239.255.0.1'))]
        receiver.recvmsg.return_value = (spdp(addresses), ancillary, 0, (LOCAL, 50000))
        argv = [str(SCRIPT), '--local', LOCAL, '--seconds', '1']
        if native:
            argv.append('--rustdds-native')
        with patch('sys.argv', argv), patch('socket.socket', return_value=receiver), \
             patch('socket.if_indextoname', return_value='eth3'), \
             patch('time.monotonic', side_effect=[0, 0, 2]), contextlib.redirect_stdout(io.StringIO()) as output:
            runpy.run_path(str(SCRIPT), run_name='__main__')
        return output.getvalue()

    def test_native_reports_multiple_addresses_and_actual_ttl(self):
        result = self.observe(True, 42, [LOCAL, '192.0.2.9'])
        self.assertIn('192.0.2.9', result)
        self.assertIn('"ttl": 42', result)
        self.assertIn('Observed 1', result)

    def test_native_requires_lan_discovery_locator(self):
        with self.assertRaisesRegex(SystemExit, 'local address missing'):
            self.observe(True, 1, ['192.0.2.9'])

    def test_default_accepts_existing_strict_configuration(self):
        self.assertIn('Observed 1', self.observe(False, 1, [LOCAL]))

    def test_default_rejects_other_addresses_and_ttl(self):
        with self.assertRaisesRegex(SystemExit, 'Unrelated address'):
            self.observe(False, 1, [LOCAL, '192.0.2.9'])
        with self.assertRaisesRegex(SystemExit, 'Expected TTL=1'):
            self.observe(False, 42, [LOCAL])


if __name__ == '__main__':
    unittest.main()
