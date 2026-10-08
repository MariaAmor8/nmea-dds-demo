# Descubrimiento multicast y validación WSL → VMware

WSL mirrored y VMware Bridged comparten LAN. Verificar `ip -br -4 addr` e
`ip route` en cada Linux. OpenDDS selecciona dirección/interfaz con `--local`.
RustDDS original enumera sus interfaces; no admite `--local` ni impone selección. En la preparación actual WSL usa 192.168.10.15 en eth3; VM usa 192.168.10.33.
Ambos binarios admiten ambos roles en una red compatible. En este entorno solo se
valida publicador WSL → suscriptor VM; no ejecutar pruebas con roles invertidos.

## Ejecutar por separado cada implementación

En VM, elegir una implementación y comenzar el suscriptor:

```bash
bash run.sh rustdds subscriber
# O, después de compilar OpenDDS:
bash run.sh opendds subscriber --local 192.168.10.33
```

En WSL, usar la misma implementación:

```bash
bash run.sh rustdds publisher
# O:
bash run.sh opendds publisher --local 192.168.10.15 --duration 65
```

Para NMEA, añadir al publicador `--source nmea --nmea-listen 127.0.0.1:3100` y
usar el simulador existente. Mantener 60 segundos de muestras, guardar ambos logs,
reiniciar el suscriptor y comprobar recuperación. Repetir con publicador iniciado
primero sin cambiar roles. Confirmar secuencia, campos y NMEA original recibido.
El agente no tiene acceso a VMware: su IP o el log PUBLICADO no prueban entrega.

## Puertos y firewall

| Implementación, dominio 0 | UDP multicast | UDP unicast, un participante por host |
| --- | --- | --- |
| OpenDDS | 239.255.0.1:7400, SPDP | 7410 SPDP, 7412 SEDP, 7411 datos/ACK |
| RustDDS | 239.255.0.1:7400 SPDP; 7401 según política nativa | 7410 descubrimiento, 7411 datos/ACK |

Si el ID RustDDS cambia, los puertos nominales son 7410+2×ID y 7411+2×ID; el
puerto de datos puede ser dinámico si está ocupado. Cerrar otros demos para mantener
ID 0. No ejecutar ambas implementaciones a la vez.

En firewalls activos, permitir únicamente UDP desde el host del demo hacia los
puertos anteriores y el grupo multicast; permitir la salida equivalente cuando
esté restringida. En Windows revisar reglas Windows y Hyper-V de WSL; en Ubuntu,
UFW si está activo. Ejemplo VM para SPDP:

```bash
sudo ufw allow from 192.168.10.15 to 239.255.0.1 port 7400 proto udp
```

Añadir 7410/7411 (RustDDS), 7412 (OpenDDS) y 7401 multicast si RustDDS lo usa.
Conservar las reglas NMEA existentes. No desactivar firewalls ni cambiar
configuraciones automáticamente. La guía OpenDDS incluye ejemplos Windows.

## Observar desde WSL

Mientras corre el publicador, observar multicast nativo local sin reenviar:

```bash
python3 tools/capture_spdp.py --local 192.168.10.15 --seconds 15 --rustdds-native
# Para OpenDDS, conservar el modo predeterminado:
python3 tools/capture_spdp.py --local 192.168.10.15 --seconds 15
sudo tcpdump -ni eth3 -vv 'udp and src host 192.168.10.15 and dst host 239.255.0.1 and dst port 7400'
```

El modo RustDDS original registra TTL, interfaz y todas las IPs anunciadas; exige
un locator de descubrimiento con la IP de la LAN observada, sin exigir exclusividad
ni un TTL configurado por el demo. `--local` pertenece al observador y no configura
DDS. El modo predeterminado de OpenDDS conserva sus comprobaciones de TTL 1 y
locators de la dirección seleccionada.
IP_PKTINFO identifica la interfaz del paquete local; tcpdump permite comprobar salida cuando hay permisos. En la VM, el usuario puede capturar esos
mismos anuncios en su interfaz real y guardar el log del suscriptor. No se prueban
anuncios VM→WSL ni se invierten los roles.

Aunque las muestras solo viajen WSL→VM, DDS Reliable necesita tráfico de control,
descubrimiento y ACK de vuelta. Una asimetría puede impedir asociación o entrega:
registrarla y distinguirla de la emisión multicast. No reintroducir peers/puente.

## Pruebas dentro de WSL

```bash
cargo test --locked -p marine-rustdds
cargo build --locked -p marine-rustdds
python3 rustdds/tests/multicast.py
MARINE_TEST_LOCAL=192.168.10.15 bash opendds/scripts/build.sh --integration-tests
```

Estas pruebas ejecutan ambos roles en el mismo Linux mediante multicast. No
constituyen validación remota ni pruebas del sentido VMware→WSL.

OpenDDS 3.34 anuncia `127.0.0.1:12345` como locators de datos ficticios en SPDP;
su código `Spdp::build_local_pdata` indica que no se usan. Los locators de datos
reales se intercambian por SEDP. El observador reconoce únicamente ese caso
exacto y exige que el locator SEDP anuncie la IPv4 seleccionada.
