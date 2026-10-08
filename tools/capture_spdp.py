#!/usr/bin/env python3
"""Observe local SPDP; never forward packets or configure remote peers."""
import argparse
import json
import socket
import struct
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--local', required=True, help='IPv4 of the observation interface; does not configure DDS')
p.add_argument('--seconds', type=float, default=15)
p.add_argument('--rustdds-native', action='store_true', help='Observe original RustDDS: report TTL and allow multiple advertised IPs')
a = p.parse_args()
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as rx:
    rx.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    rx.bind(('', 7400))
    rx.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                  socket.inet_aton('239.255.0.1') + socket.inet_aton(a.local))
    rx.setsockopt(socket.IPPROTO_IP, 8, 1)  # Linux IP_PKTINFO
    rx.setsockopt(socket.IPPROTO_IP, 12, 1)  # Linux IP_RECVTTL
    rx.settimeout(0.5)
    deadline = time.monotonic() + a.seconds
    observed = 0
    while time.monotonic() < deadline:
        try:
            data, ancillary, _, source = rx.recvmsg(65535, 1024)
        except socket.timeout:
            continue
        if source[0] != a.local or data[:4] != b'RTPS':
            continue
        ttl = next((struct.unpack('i', payload)[0] for level, kind, payload in ancillary
                    if level == socket.IPPROTO_IP and kind == socket.IP_TTL), None)
        pktinfo = next(payload for level, kind, payload in ancillary
                       if level == socket.IPPROTO_IP and kind == 8)
        interface = socket.if_indextoname(struct.unpack_from('i', pktinfo)[0])
        destination = socket.inet_ntoa(pktinfo[8:12])
        if destination != '239.255.0.1':
            raise SystemExit('Unexpected destination')
        # SPDP uses parameter-list CDR. Collect IPv4 locators from DATA payloads.
        locators = []
        spdp_sample = False
        offset = 20
        while offset + 4 <= len(data):
            kind, flags = data[offset:offset + 2]
            endian = '<' if flags & 1 else '>'
            length = struct.unpack_from(endian + 'H', data, offset + 2)[0]
            end = offset + 4 + length if length else len(data)
            if kind == 0x15 and flags & 4 and data[offset + 12:offset + 16] == b'\x00\x01\x00\xc2' and offset + 24 <= end:  # DATA, no inline QoS in normal SPDP
                inline_offset = struct.unpack_from(endian + 'H', data, offset + 6)[0]
                payload = offset + 8 + inline_offset
                if not flags & 2 and payload + 4 <= end:
                    spdp_sample = True
                    representation = data[payload:payload + 2]
                    cdr_endian = '<' if representation == b'\x00\x03' else '>'
                    pos = payload + 4
                    while pos + 4 <= end:
                        pid, size = struct.unpack_from(cdr_endian + 'HH', data, pos)
                        pos += 4
                        if pid == 1:
                            break
                        if pos + size > end:
                            break
                        if pid in (0x31, 0x32, 0x33, 0x48) and size == 24:
                            locator_kind, port = struct.unpack_from(cdr_endian + 'iI', data, pos)
                            if locator_kind == 1:
                                locators.append({'pid': hex(pid), 'ip': socket.inet_ntoa(data[pos + 20:pos + 24]), 'port': port})
                        pos += (size + 3) & ~3
            offset = end
        if not spdp_sample:
            continue
        print(json.dumps({'source': source, 'group': '239.255.0.1', 'port': 7400,
                          'ttl': ttl, 'interface': interface, 'guid_prefix': data[8:20].hex(), 'locators': locators}), flush=True)
        if not a.rustdds_native and ttl != 1:
            raise SystemExit('Expected TTL=1')
        if not any(x['pid'] == '0x32' and x['ip'] == a.local for x in locators):
            raise SystemExit('Selected local address missing from SPDP locators')
        # OpenDDS 3.34 Spdp::build_local_pdata uses dummy default data locators;
        # real publication/subscription transport locators are sent via SEDP.
        effective = [x for x in locators if not (data[6:8] == b'\x01\x03'
                     and x['pid'] in ('0x31', '0x48')
                     and x['ip'] == '127.0.0.1' and x['port'] == 12345)]
        if not a.rustdds_native and any(x['ip'] not in (a.local, '239.255.0.1') for x in effective):
            raise SystemExit('Unrelated address advertised in SPDP locators')
        observed += 1
    if not observed:
        raise SystemExit('No local SPDP observed')
    print(f'Observed {observed} local RTPS/SPDP datagrams', flush=True)
