#!/usr/bin/env python3
"""Real OpenDDS same-host multicast tests; run with the OpenDDS DLL/library environment loaded."""
import argparse
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time

VALID = 'GPS1 on UDP2: $GPRMC,131537.83,A,0000.95081,S,00000.29512,W,0010.0,089.0,051026,0.0,W,A,S*66'


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class Harness:
    def __init__(self, binaries, logs):
        self.binaries = binaries
        self.logs = logs
        self.processes = []

    def start(self, role, name, duration=0, extra=()):
        base = 17410 if role == 'publisher' else 17510
        suffix = '.exe' if os.name == 'nt' else ''
        command = [str(self.binaries / (role + suffix)), '--spdp-port', str(base),
                   '--sedp-port', str(base + 2), '--data-port', str(base + 1),
                   '--duration', str(duration), *extra]
        path = self.logs / (name + '.log')
        output = path.open('w', encoding='utf-8')
        try:
            process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == 'nt' else 0)
        finally:
            output.close()
        self.processes.append(process)
        return process, path

    def wait_for(self, process, path, needle, timeout=15):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            text = path.read_text(encoding='utf-8', errors='replace')
            if needle in text:
                return text
            if process.poll() is not None:
                raise AssertionError(f'{needle!r} missing; process exited {process.returncode}: {text}')
            time.sleep(0.1)
        raise AssertionError(f'timeout waiting for {needle!r}: {path.read_text(errors="replace")}')

    @staticmethod
    def finish(process, timeout=20):
        require(process.wait(timeout=timeout) == 0, f'process failed: {process.returncode}')

    @staticmethod
    def interrupt(process):
        process.send_signal(signal.CTRL_BREAK_EVENT if os.name == 'nt' else signal.SIGINT)
        Harness.finish(process, 10)

    def cleanup(self):
        for process in self.processes:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def compare(pub, sub, minimum=2):
    published = re.findall(r'^PUBLICADO DDS: (.*)$', pub.read_text(errors='replace'), re.M)
    received = re.findall(r'^PARSEADO: (.*)$', sub.read_text(errors='replace'), re.M)
    require(len(received) >= minimum, f'not enough received samples: {sub}')
    require(all(row in published for row in received), 'DDS fields differ between writer and reader')
    sequences = [int(re.search(r'seq=(\d+)', row)[1]) for row in received]
    require(all(b == ((a + 1) & 0xffffffff) for a, b in zip(sequences, sequences[1:])), 'unexpected sequence gap')
    require('SALTO' not in sub.read_text(errors='replace'), 'sequence gap reported')


def run(h):
    # Both start orders, synthetic samples and finite-duration cleanup.
    for publisher_first in (False, True):
        name = 'publisher-first' if publisher_first else 'subscriber-first'
        first_role = 'publisher' if publisher_first else 'subscriber'
        first, first_log = h.start(first_role, name + '-first', 9)
        h.wait_for(first, first_log, 'listo.')
        second_role = 'subscriber' if publisher_first else 'publisher'
        second, second_log = h.start(second_role, name + '-second', 6)
        reader, reader_log = (second, second_log) if publisher_first else (first, first_log)
        h.wait_for(reader, reader_log, 'PARSEADO:')
        h.finish(second)
        h.finish(first)
        pub, sub = (first_log, second_log) if publisher_first else (second_log, first_log)
        compare(pub, sub)
        require('simulated=true' in sub.read_text(), 'synthetic flag missing')
        print(name, 'passed', flush=True)

    # Interrupt and restart a reader while the same publisher keeps running.
    publisher, publog = h.start('publisher', 'restart-publisher')
    reader, readerlog = h.start('subscriber', 'restart-reader-1')
    h.wait_for(reader, readerlog, 'PARSEADO:')
    h.interrupt(reader)
    reader2, readerlog2 = h.start('subscriber', 'restart-reader-2')
    h.wait_for(reader2, readerlog2, 'PARSEADO:')
    time.sleep(1.2)
    h.interrupt(reader2)
    h.interrupt(publisher)
    compare(publog, readerlog, minimum=1)
    compare(publog, readerlog2, minimum=1)
    print('reader restart / signal cleanup passed', flush=True)

    # NMEA datagram with valid, invalid, valid lines; rejected checksum uses no sequence.
    reader, sublog = h.start('subscriber', 'nmea-reader')
    publisher, publog = h.start('publisher', 'nmea-publisher', extra=('--source', 'nmea', '--nmea-listen', '127.0.0.1:17300'))
    h.wait_for(publisher, publog, 'Publicador listo.')
    h.wait_for(publisher, publog, 'PublicationMatched lectores=1')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.sendto((VALID + '\r\n' + VALID.replace('*66', '*67') + '\n' + VALID + '\n').encode(), ('127.0.0.1', 17300))
    h.wait_for(reader, sublog, 'PARSEADO: seq=2 ')
    h.interrupt(publisher)
    h.interrupt(reader)
    compare(publog, sublog)
    require(publog.read_text().count('PUBLICADO DDS:') == 2, 'invalid sentence published')
    require('GPRMC rechazada:' in publog.read_text(), 'missing rejection')
    require(sublog.read_text().count('RECIBIDO DDS: ' + VALID) == 2, 'raw NMEA changed')
    require('heading=N/D depth=N/D position_valid=true heading_valid=false depth_valid=false simulated=false' in sublog.read_text(), 'NMEA flags changed')
    print('NMEA multiline / raw line / flags passed', flush=True)

    # Occupied NMEA port and retired interface selection must fail.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
        if os.name == 'nt':
            occupied.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        occupied.bind(('127.0.0.1', 17300))
        publisher, path = h.start('publisher', 'occupied-nmea', 3, ('--source', 'nmea', '--nmea-listen', '127.0.0.1:17300'))
        require(publisher.wait(timeout=15) != 0, 'occupied NMEA port accepted')
        require('no se pudo abrir NMEA UDP' in path.read_text(errors='replace'), 'wrong occupied port error')
    publisher, path = h.start('publisher', 'retired-local', 2, ('--local', '127.0.0.1'))
    require(publisher.wait(timeout=15) != 0, 'retired --local accepted')
    require('opcion desconocida: --local' in path.read_text(errors='replace'), 'wrong retired option error')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
        if os.name == 'nt':
            occupied.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        occupied.bind(('0.0.0.0', 17410))
        publisher, _ = h.start('publisher', 'occupied-spdp', 2)
        require(publisher.wait(timeout=15) != 0, 'occupied SPDP port accepted')
    # Prove UDP ports are reusable after both finite duration and signal shutdown.
    for port in (17410, 17411, 17412, 17510, 17511, 17512, 17300):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            if os.name == 'nt':
                probe.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            probe.bind(('127.0.0.1' if port == 17300 else '0.0.0.0', port))
    print('failure modes / port reuse passed', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', required=True, type=Path)
    args = parser.parse_args()
    logs = Path(tempfile.mkdtemp(prefix='marine-opendds-tests-'))
    print('Logs:', logs, flush=True)
    h = Harness(args.bin_dir.resolve(), logs)
    try:
        run(h)
    finally:
        h.cleanup()
    print('OpenDDS same-host multicast integration passed', flush=True)


if __name__ == '__main__':
    main()
