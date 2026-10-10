"""Test the common SPDP observer with simulated packets, without real sockets."""
import contextlib
import io
import json
from pathlib import Path
import runpy
import socket
import struct
import unittest
from unittest.mock import MagicMock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'capture_spdp.py'
LOCAL = '192.168.10.15'


def spdp(locators, vendor=b'\x01\x12'):
    parameters = b''
    for pid, address, port in locators:
        parameters += struct.pack('<HHiI', pid, 24, 1, port)
        parameters += bytes(12) + socket.inet_aton(address)
    payload = b'\x00\x03\x00\x00' + parameters + struct.pack('<HH', 1, 0)
    body = struct.pack('<HH', 0, 16) + bytes(4) + b'\x00\x01\x00\xc2' + bytes(8) + payload
    return b'RTPS\x02\x03' + vendor + bytes(12) + struct.pack('<BBH', 0x15, 5, len(body)) + body


class ObservationTest(unittest.TestCase):
    def observe(self, locators=(), ttl=1, native=False, source=LOCAL,
                packet=None, vendor=b'\x01\x12', timeout=False):
        receiver = MagicMock()
        receiver.__enter__.return_value = receiver
        ancillary = [(socket.IPPROTO_IP, socket.IP_TTL, struct.pack('i', ttl)),
                     (socket.IPPROTO_IP, 8, struct.pack('i', 3) + socket.inet_aton(LOCAL) + socket.inet_aton('239.255.0.1'))]
        receiver.recvmsg.return_value = (
            spdp(locators, vendor) if packet is None else packet,
            ancillary, 0, (source, 50000))
        if timeout:
            receiver.recvmsg.side_effect = socket.timeout
        argv = [str(SCRIPT), '--local', LOCAL, '--seconds', '1']
        if native:
            argv.append('--rustdds-native')
        with patch('sys.argv', argv), patch('socket.socket', return_value=receiver), \
             patch('socket.if_indextoname', return_value='ens33'), \
             patch('time.monotonic', side_effect=[0, 0, 2]), contextlib.redirect_stdout(io.StringIO()) as output:
            runpy.run_path(str(SCRIPT), run_name='__main__')
        self.assertIn('Observed 1 local RTPS/SPDP datagrams', output.getvalue())
        return json.loads(output.getvalue().splitlines()[0])

    def test_single_locator(self):
        result = self.observe([(0x32, LOCAL, 7410)])
        self.assertEqual(result['locators'], [{'pid': '0x32', 'ip': LOCAL, 'port': 7410}])
        self.assertEqual(result['interface'], 'ens33')
        self.assertEqual(result['ttl'], 1)

    def test_multiple_locators_and_observed_ttl_for_both_vendors(self):
        locators = [(0x32, LOCAL, 7410), (0x32, '192.0.2.9', 7410)]
        for vendor in (b'\x01\x12', b'\x01\x03'):
            with self.subTest(vendor=vendor):
                result = self.observe(locators, ttl=42, vendor=vendor)
                self.assertEqual(result['ttl'], 42)
                self.assertEqual([x['ip'] for x in result['locators']], [LOCAL, '192.0.2.9'])

    def test_observed_ip_need_not_be_advertised(self):
        result = self.observe([(0x32, '192.0.2.9', 7410)])
        self.assertEqual(result['source'][0], LOCAL)
        self.assertEqual(result['locators'][0]['ip'], '192.0.2.9')

    def test_opendds_dummy_locators_are_preserved(self):
        result = self.observe([(0x31, '127.0.0.1', 12345),
                               (0x48, '127.0.0.1', 12345),
                               (0x32, LOCAL, 7412)], vendor=b'\x01\x03')
        self.assertEqual(len(result['locators']), 3)
        self.assertEqual(result['locators'][0], {'pid': '0x31', 'ip': '127.0.0.1', 'port': 12345})
        self.assertEqual(result['locators'][1], {'pid': '0x48', 'ip': '127.0.0.1', 'port': 12345})

    def test_legacy_flag_has_no_effect(self):
        locators = [(0x32, '192.0.2.9', 7410)]
        self.assertEqual(self.observe(locators, ttl=42), self.observe(locators, ttl=42, native=True))

    def test_other_source_is_not_counted(self):
        with self.assertRaisesRegex(SystemExit, 'No local SPDP observed'):
            self.observe([(0x32, LOCAL, 7410)], source='192.0.2.9')

    def test_non_spdp_packets_are_not_counted(self):
        for packet in (b'not RTPS', b'RTPS\x02\x03\x01\x12' + bytes(12)):
            with self.subTest(packet=packet), self.assertRaisesRegex(SystemExit, 'No local SPDP observed'):
                self.observe(packet=packet)

    def test_no_announcements_is_an_error(self):
        with self.assertRaisesRegex(SystemExit, 'No local SPDP observed'):
            self.observe(timeout=True)


if __name__ == '__main__':
    unittest.main()
