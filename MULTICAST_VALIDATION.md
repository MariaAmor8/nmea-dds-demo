# Validación histórica de la implementación con parche — 2026-10-08

> RustDDS fue sustituido por el paquete original de crates.io. Los resultados
> siguientes describen el parche retirado; no validan RustDDS original.
> La nueva evidencia está en [RUSTDDS_NATIVE_VALIDATION.md](RUSTDDS_NATIVE_VALIDATION.md).

Rama: `feature/multicast-discovery`, creada desde `main`.
Entorno: WSL mirrored, 192.168.10.15/24, interfaz eth3, ruta por 192.168.10.1.
VMware Bridged, VM 192.168.10.33 según información del usuario; sin acceso a su proceso.

## Comprobaciones locales

- RustDDS 0.11.2 parcheado: compilación `--locked`, cuatro pruebas unitarias
  (GPRMC, checksum, IPv4 e interfaces inválidas) y una prueba CDR aprobadas.
- Integración RustDDS dentro de WSL: ambos órdenes de inicio, entrega sintética,
  reinicio del lector, NMEA válido/inválido y rechazo de argumentos retirados.
- OpenDDS 3.34.0: compilación, fuentes/configuración, CDR e integración multicast
  dentro de WSL. La integración verifica ambos órdenes de inicio, reinicio,
  NMEA y muestras sintéticas, IP inválida, puertos ocupados y cierre.
- Bash y scripts Python revisados; `git diff --check` sin errores.

RustDDS puede entregar muestras de caché durante reasociación y presentar un salto
antes de continuar con muestras nuevas. La prueba de reinicio valida recuperación
y continuidad de muestras nuevas después de ese arranque; no promete continuidad
del historial durante el reinicio. QoS y lógica de caché conservan su comportamiento.

La primera ejecución de las dos integraciones se solapó por error; se descartó
como evidencia y se repitieron por separado. La ejecución independiente RustDDS
pasó en `/tmp/marine-rustdds-multicast-1tfm4ybi/`.

## Emisión SPDP desde WSL

El observador `tools/capture_spdp.py` recibe directamente el grupo multicast local,
sin reenviar tráfico. Verifica origen, destino, TTL e IPs anunciadas. IP_PKTINFO
identifica la interfaz de recepción del paquete multicast local. Esto no equivale
a una captura en la tarjeta de la VM ni prueba que el paquete atravesó la LAN.

Observación aprobada por separado en ambas bibliotecas: RustDDS emitió tres
anuncios SPDP completos y OpenDDS ocho durante ventanas de seis segundos;
ambos con origen 192.168.10.15, destino 239.255.0.1:7400, TTL 1 e interfaz eth3.
RustDDS anunció puertos 7410/7411; OpenDDS anunció SEDP en 7412. OpenDDS conserva
sus locators ficticios de datos 127.0.0.1:12345 en SPDP, que no usa para transportar
muestras: los reales viajan por SEDP.

Evidencia local (archivos temporales): `/tmp/multicast-rust-spdp.log`,
`/tmp/multicast-opendds-spdp.log`, `/tmp/multicast-rust-tests.log`,
`/tmp/multicast-rust-integration.log` y `/tmp/multicast-opendds-tests.log`.
OpenDDS se repitió sin solapamiento: sus tres pruebas aprobaron en 23.28 segundos.

Las capturas raw con tcpdump quedaron pendientes: falta CAP_NET_RAW y sudo exige
autenticación interactiva. No se modificaron permisos ni firewalls.

## Recepción remota

**Pendiente, sin afirmar entrega en VMware.** El usuario reportó socat WSL→VM
funcional y el sentido contrario fallido. No se ejecutaron pruebas VM→WSL ni
se invirtieron roles remotos. No se obtuvo evidencia del suscriptor de la VM.

Usar [MULTICAST.md](MULTICAST.md) para verificar 60 segundos de entrega sintética,
NMEA y recuperación con publicador WSL y suscriptor VM. Las pruebas locales
no sustituyen esa aceptación. DDS puede requerir control/ACK de vuelta.

Los resultados históricos de OpenDDS están en [opendds/VALIDATION.md](opendds/VALIDATION.md)
y describen la implementación anterior; no validan la red de esta rama.
