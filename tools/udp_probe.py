"""Prueba UDP unicast con respuesta. Puerto 17400, independiente de DDS."""
import argparse
import socket
p=argparse.ArgumentParser()
p.add_argument('role', choices=['listen','send'])
p.add_argument('--local', required=True)
p.add_argument('--peer')
a=p.parse_args()
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
    s.bind((a.local,17400));s.settimeout(60 if a.role=='listen' else 5)
    try:
        if a.role=='listen':
            print('Esperando prueba unicast...',flush=True)
            data,origin=s.recvfrom(1024)
            if data != b'NMEA-DEMO-PROBE':
                raise RuntimeError('Mensaje de prueba inesperado')
            s.sendto(b'NMEA-DEMO-ACK',origin)
            print(f'PROBE recibido de {origin}; ACK enviado.',flush=True)
        else:
            if not a.peer:p.error('Falta --peer')
            s.sendto(b'NMEA-DEMO-PROBE',(a.peer,17400))
            data,origin=s.recvfrom(1024)
            if data != b'NMEA-DEMO-ACK' or origin != (a.peer,17400):
                raise RuntimeError(f'Respuesta inesperada desde {origin}: {data!r}')
            print(f'OK: UDP unicast funciona en ambos sentidos con {origin}.')
    except socket.timeout:
        raise SystemExit('TIMEOUT: revisa IP y firewall. No continuar con DDS hasta resolver UDP unicast.')
