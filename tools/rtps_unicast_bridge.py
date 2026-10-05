"""Reenvia RTPS LOCAL de 7400/7401 a los puertos unicast del par fijo.
No altera GUID, serializacion ni datos. No es un discovery server DDS.
Domain 0. Debe ejecutarse en Linux, con multicast local funcional.
"""
import argparse
import selectors
import socket
import sys

p = argparse.ArgumentParser()
p.add_argument("--local", required=True)
p.add_argument("--peer", required=True)
p.add_argument("--peer-id", type=int, default=0)
a = p.parse_args()
if not 0 <= a.peer_id < 120:
    p.error("peer-id debe estar entre 0 y 119")
sel = selectors.DefaultSelector()
sockets = []
try:
    tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tx.bind((a.local, 0))
    sockets.append(tx)
    for multicast_port, offset in [(7400, 10), (7401, 11)]:
        rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sockets.append(rx)
        rx.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        rx.bind(("", multicast_port))
        rx.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                      socket.inet_aton("239.255.0.1") + socket.inet_aton(a.local))
        rx.setblocking(False)
        sel.register(rx, selectors.EVENT_READ, 7400 + offset + 2 * a.peer_id)
    print("READY", flush=True)
    counts = {7410 + 2*a.peer_id: 0, 7411 + 2*a.peer_id: 0}
    while True:
        for key, _ in sel.select(timeout=1):
            data, origin = key.fileobj.recvfrom(65535)
            # Solo paquetes del participante LOCAL. No reenviar trafico remoto.
            if origin[0] != a.local or not data.startswith(b"RTPS"):
                continue
            tx.sendto(data, (a.peer, key.data))
            counts[key.data] += 1
            if counts[key.data] == 1:
                print(f"[RTPS bridge] {a.local} -> {a.peer}:{key.data}, {len(data)} bytes", file=sys.stderr, flush=True)
except KeyboardInterrupt:
    pass
finally:
    sel.close()
    for s in sockets:
        s.close()
