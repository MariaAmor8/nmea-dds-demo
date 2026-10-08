#!/usr/bin/env python3
"""Same-host RustDDS discovery and delivery; does not access VMware."""
import argparse
import re
import signal
import socket
import subprocess
import tempfile
import time
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin-dir', type=Path, default=Path('target/debug'))
a = p.parse_args()
logs = Path(tempfile.mkdtemp(prefix='marine-rustdds-multicast-'))
print('Logs:', logs, flush=True)
processes = []

def start(role, name, extra=()):
    path = logs / (name + '.log')
    with path.open('w') as output:
        process = subprocess.Popen([str(a.bin_dir.resolve() / role), *extra], stdout=output, stderr=subprocess.STDOUT)
    processes.append(process)
    return process, path

def wait(process, path, needle, seconds=25):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        text = path.read_text()
        if needle in text:
            return
        if process.poll() is not None:
            raise AssertionError(text)
        time.sleep(.1)
    raise AssertionError(f'{needle} missing: {path.read_text()}')

def wait_new_samples(process, path, after, seconds=25):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        samples = [int(x) for x in re.findall(r'PARSEADO: seq=(\d+)', path.read_text()) if int(x) > after]
        if len(samples) >= 3:
            return
        if process.poll() is not None:
            raise AssertionError(path.read_text())
        time.sleep(.1)
    raise AssertionError(f'No recovery after sequence {after}: {path.read_text()}')

def stop(process):
    process.send_signal(signal.SIGINT)
    assert process.wait(timeout=10) == 0

def compare(pub, sub, live_after=None):
    sent = set(re.findall(r'PUBLICADO DDS: seq=(\d+)', pub.read_text()))
    received = re.findall(r'PARSEADO: seq=(\d+)', sub.read_text())
    assert received and set(received) <= sent, (pub, sub)
    if live_after is not None:
        # RustDDS can replay cached samples during re-association; evaluate new live samples.
        received = [x for x in received if int(x) > live_after]
        assert len(received) >= 3, received
    assert all(int(b) == int(a) + 1 for a, b in zip(received, received[1:])), received

try:
    for flag in ('--local', '--peer', '--peer-id', '--no-bridge'):
        process, path = start('publisher', 'retired-' + flag, (flag, '0'))
        assert process.wait(timeout=5) != 0 and 'Opcion desconocida' in path.read_text()
    for publisher_first in (True, False):
        # Same host uses different participant IDs; this is not a remote peer ID.
        first_role = 'publisher' if publisher_first else 'subscriber'
        second_role = 'subscriber' if publisher_first else 'publisher'
        first, first_log = start(first_role, first_role + '-first')
        wait(first, first_log, 'listo.')
        second, second_log = start(second_role, second_role + '-second', ('--expected-id', '1'))
        pub, publog = (first, first_log) if publisher_first else (second, second_log)
        sub, sublog = (second, second_log) if publisher_first else (first, first_log)
        wait(sub, sublog, 'PARSEADO:')
        time.sleep(3)
        stop(sub)
        sub2, sublog2 = start('subscriber', 'restart-' + first_role,
                             ('--expected-id', '1' if publisher_first else '0'))
        checkpoint = int(re.findall(r'PUBLICADO DDS: seq=(\d+)', publog.read_text())[-1])
        wait_new_samples(sub2, sublog2, checkpoint + 4)
        time.sleep(3)
        stop(sub2)
        stop(pub)
        compare(publog, sublog)
        compare(publog, sublog2, live_after=checkpoint + 4)
    sub, sublog = start('subscriber', 'nmea-subscriber')
    wait(sub, sublog, 'listo.')
    pub, publog = start('publisher', 'nmea-publisher', ('--expected-id', '1', '--source', 'nmea', '--nmea-listen', '127.0.0.1:17300'))
    wait(pub, publog, 'PublicationMatched')
    valid = '$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A'
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.sendto((valid + '\n' + valid[:-2] + '00\n' + valid + '\n').encode(), ('127.0.0.1', 17300))
    wait(sub, sublog, 'PARSEADO: seq=2 ')
    stop(pub)
    stop(sub)
    compare(publog, sublog)
    assert sublog.read_text().count('RECIBIDO DDS: ' + valid) == 2
    print('RustDDS same-host multicast integration passed', flush=True)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
