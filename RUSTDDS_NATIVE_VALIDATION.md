# RustDDS original: validación — 2026-10-08

Corrección sobre `feature/multicast-discovery`. RustDDS **0.11.2 de crates.io**,
sin `vendor/rustdds`, `[patch.crates-io]`, copia local ni modificaciones de la biblioteca.
El participante se crea con `DomainParticipant::new(0)`; el demo no selecciona
interfaces ni configura TTL, locators o sockets. `--local` se retiró de RustDDS.

## Dependencia y compilación

- `cargo build --offline --locked -p marine-rustdds`: aprobado.
- `cargo test --offline --locked -p marine-rustdds`: dos pruebas GPRMC/checksum y
  una prueba CDR aprobadas.
- Metadatos Cargo para Linux: versión 0.11.2, origen
  `registry+https://github.com/rust-lang/crates.io-index`; manifiesto en la caché
  de Cargo, no en el repositorio. Cargo.lock volvió al contenido original sin
  actualizar dependencias ajenas al cambio.
- Checksum del archivo publicado verificado:
  `c59c50cd4a3d62966486f8a0a818ca1312fc5b5aa56669a039dda2fbfe9a1fb9`.
  Los archivos de código fuente usados desde la caché coinciden con el paquete.

La caché estándar de Cargo contiene la biblioteca descargada, como cualquier
dependencia; no es una copia propia mantenida en este repositorio.

## Ejecución local y observación

`python3 rustdds/tests/multicast.py`: aprobado dentro de WSL, sin `--local`.
Verifica ambos órdenes de arranque, asociación y entrega sintética, reinicio del
lector, continuidad de muestras nuevas tras reasociación, NMEA válido/inválido y
rechazo de `--local`, `--peer`, `--peer-id` y `--no-bridge`.
Logs de procesos: `/tmp/marine-rustdds-multicast-lax1knok/`.
No se cambia la caché ni el QoS; no se promete continuidad del historial durante
el reinicio del lector.

Observación SPDP con `--rustdds-native`: aprobada en 192.168.10.15, interfaz eth3,
destino 239.255.0.1:7400. TTL observado: 1, establecido por el comportamiento
nativo, no por el demo. Se observaron locators de **192.168.10.15 y 10.255.255.254**;
ambos se conservan. Los dos participantes locales anunciaron puertos nominales
7410/7411 y 7412/7413 según su ID, y grupos multicast 7400/7401.

El observador selecciona dónde recibir paquetes, sin configurar el participante.
IP_PKTINFO muestra la interfaz del paquete recibido localmente; no prueba tránsito
por la LAN ni recepción en VMware. El modo RustDDS permite múltiples IPs y registra
el TTL real. Cuatro pruebas del observador aprobadas, incluyendo conservación de
las comprobaciones predeterminadas para OpenDDS.

Evidencia temporal: `/tmp/native-rust-build.log`, `/tmp/native-rust-tests.log`,
`/tmp/native-rust-integration.log`, `/tmp/native-rust-spdp.log` y
`/tmp/native-rust-metadata.json`.

## Alcance y pendientes

Fuentes, configuración, pruebas y documentación de OpenDDS comparadas mediante
SHA-256 antes/después: **sin cambios durante esta corrección**. Su validación
anterior no se ha repetido ni sustituido.

Recepción VMware 192.168.10.33: **pendiente de evidencia del usuario**, sin acceso
a su proceso. No se invirtieron roles remotos ni se probó multicast VM→WSL.
Los comandos están en [MULTICAST.md](MULTICAST.md). La entrega de 60 segundos y
NMEA entre máquinas requiere los logs del suscriptor, aunque la prueba local pase.
DDS Reliable puede necesitar tráfico de descubrimiento/control y ACK de vuelta.

La evidencia anterior del parche está conservada explícitamente como histórica
en [MULTICAST_VALIDATION.md](MULTICAST_VALIDATION.md); no valida esta implementación.
