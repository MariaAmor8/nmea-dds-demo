# Red y diagnóstico multicast entre dos MV Ubuntu

## 1. Preparar VMware y comprobar las direcciones

En el host físico, configurar **ambas MV en Bridged**. Abrir el editor de red virtual de VMware, localizar la red puente utilizada por las MV y seleccionar explícitamente el adaptador físico con conexión a Internet. En la prueba reportada fue Intel(R) WiFi; usar el nombre real del equipo, no copiar ese nombre ni asumir que Automatic elige correctamente.

En **ambas MV**:

```bash
cat /etc/os-release
ip -br -4 addr
ip route
ip link
```

Resultado esperado: cada MV tiene una IPv4 distinta en la LAN del adaptador puente, una ruta apropiada hacia la otra MV y una interfaz activa con capacidad `MULTICAST`. No usar `127.0.0.1` como dirección DDS entre MV. El nombre de interfaz puede ser, por ejemplo, `ens33`; leer el real.

Para los ejemplos siguientes, definir en **cada terminal**, sustituyendo los valores:

```bash
export DEMO_LOCAL_IP="192.168.10.15"  # IPv4 de ESTA MV
export DEMO_PEER_IP="192.168.10.33"   # IPv4 de la OTRA MV
export DEMO_INTERFACE="ens33"        # interfaz de ESTA MV
```

En la otra MV los valores local/remoto se intercambian. Son variables auxiliares de los comandos, no configuración leída automáticamente por DDS. La red debe permitir multicast entre las MV; el descubrimiento OpenDDS usa TTL 1 y no está diseñado aquí para atravesar routers.

## 2. Protocolos y puertos

| Implementación, dominio 0 | UDP multicast | UDP unicast local |
| --- | --- | --- |
| RustDDS | `239.255.0.1:7400` SPDP; `7401` según política nativa | `7410` descubrimiento y `7411` datos/control para ID 0 |
| OpenDDS | `239.255.0.1:7400` SPDP | `7410` SPDP, `7412` SEDP, `7411` datos/control |
| Fuente NMEA opcional | No utiliza multicast | `127.0.0.1:3100` predeterminado, solo publicador |
| Probe de diagnóstico | No utiliza multicast | `17400`, prueba UDP con respuesta |

RustDDS usa puertos nominales `7410 + 2 × ID` y `7411 + 2 × ID`; el puerto de datos puede ser dinámico si está ocupado. Mantener un participante por MV, ID 0, y cerrar otras aplicaciones del dominio. `--expected-id` comprueba el ID, no lo asigna. OpenDDS admite puertos personalizados; ajustar firewall y capturas si se cambian.

OpenDDS usa multicast para SPDP y unicast para SEDP/datos. RustDDS conserva la política nativa de la biblioteca, sin selección de interfaz, peers ni puente auxiliar. Ambos necesitan descubrimiento y control de retorno; recibir un anuncio multicast por sí solo no demuestra entrega DDS.

## 3. Firewall Ubuntu

En **cada MV**, comprobar UFW si está instalado:

```bash
if command -v ufw >/dev/null; then sudo ufw status verbose; fi
```

Si no existe o está inactivo, no hace falta instalarlo ni activarlo para seguir esta guía. Si está activo, con las variables del paso 1 definidas, permitir desde la otra MV:

```bash
sudo ufw allow from "$DEMO_PEER_IP" to 239.255.0.1 port 7400 proto udp
sudo ufw allow from "$DEMO_PEER_IP" to "$DEMO_LOCAL_IP" port 7410:7411 proto udp
# RustDDS:
sudo ufw allow from "$DEMO_PEER_IP" to 239.255.0.1 port 7401 proto udp
# OpenDDS:
sudo ufw allow from "$DEMO_PEER_IP" to "$DEMO_LOCAL_IP" port 7412 proto udp
sudo ufw status numbered
```

Resultado esperado: reglas UDP para la IP de la otra MV y los destinos indicados. Si se restringe también la salida, permitir los destinos equivalentes hacia la otra MV y el grupo multicast. Actualizar reglas si cambian las IP. No desactivar el firewall como solución.

## 4. Diagnóstico unicast opcional

Desde la raíz del repositorio, primero en **MV B**:

```bash
python3 tools/udp_probe.py listen --local "$DEMO_LOCAL_IP"
```

Después en **MV A**:

```bash
python3 tools/udp_probe.py send --local "$DEMO_LOCAL_IP" --peer "$DEMO_PEER_IP"
```

Si UFW está activo, permitir antes UDP 17400 desde la otra MV en ambas máquinas. El envío debe imprimir `OK: UDP unicast funciona en ambos sentidos con ...` y el receptor termina tras responder. `TIMEOUT` indica un problema de conectividad o retorno. Esta herramienta no prueba multicast ni DDS; repetir intercambiando roles si se necesita diagnosticar ambos sentidos.

## 5. Observar el descubrimiento

Con un participante ejecutándose, desde la raíz del repositorio en cualquiera de las MV:

```bash
# RustDDS:
python3 tools/capture_spdp.py --local "$DEMO_LOCAL_IP" --seconds 15 --rustdds-native
# OpenDDS, en una prueba separada:
python3 tools/capture_spdp.py --local "$DEMO_LOCAL_IP" --seconds 15
```

`--local` selecciona dónde escucha el observador; no configura RustDDS. El modo nativo registra TTL, interfaces e IP anunciadas sin exigir un único locator. El modo OpenDDS comprueba TTL 1 y la IP seleccionada. OpenDDS puede anunciar locators ficticios `127.0.0.1:12345` en SPDP: los de datos reales se intercambian por SEDP.

Para comprobar los paquetes en la interfaz real, instalar `tcpdump` si hace falta y capturar mientras ambos procesos están activos:

```bash
sudo apt install -y tcpdump
sudo tcpdump -ni "$DEMO_INTERFACE" -vv 'udp and dst host 239.255.0.1 and dst port 7400'
sudo tcpdump -ni "$DEMO_INTERFACE" "udp and host $DEMO_PEER_IP and portrange 7410-7412"
```

Resultado esperado: anuncios SPDP y tráfico unicast con la otra MV. Una observación local no demuestra recepción remota: la evidencia de entrega son las líneas `RECIBIDO DDS` y `PARSEADO` del suscriptor remoto.

## 6. Si no funciona

| Síntoma | Comprobar |
| --- | --- |
| Sin descubrimiento/asociación | Bridged, adaptador físico seleccionado, IP/rutas, multicast 7400 y firewall de ambas MV |
| Asociación sin muestras | Fuente sintética, errores de escritura, datos/control y tráfico de retorno |
| OpenDDS rechaza `--local` | La IP debe pertenecer a una interfaz local activa con multicast |
| RustDDS informa ID inesperado | Cerrar otros participantes; no usar `--expected-id` para seleccionar una interfaz |
| Puerto ocupado | `ss -lunp`, cerrar procesos anteriores o revisar opciones de puertos OpenDDS |
| Biblioteca OpenDDS ausente | Instalación compilada, `MARINE_DDS_ROOT`, entorno y `ldd` |

Consultar las guías [RustDDS](rustdds/README.md) y [OpenDDS](opendds/README.md). Guardar comandos, versiones, IP y salidas de ambas consolas en `logs/`; capturas en `captures/`. No usar el éxito de compilación como prueba de comunicación.
