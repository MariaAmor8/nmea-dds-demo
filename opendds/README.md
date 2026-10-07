# Demo OpenDDS 3.34.0 en WSL y Ubuntu VMware

Publicador C++ en **WSL2** y suscriptor C++ en **Ubuntu VMware**. Windows
aloja WSL, ejecuta el simulador NMEA y administra el firewall. El flujo nativo
Windows/MSVC se retiró. RustDDS conserva su implementación y sus instrucciones.

Las IP `192.168.10.15` (WSL) y `192.168.10.33` (MV) son ejemplos: sustituirlas
por las direcciones verificadas en cada máquina y también en las reglas.

## Contrato y red

Dominio 0, tópico `MarineNavigation`, tipo `Marine::Navigation`, sin clave.
Reliable, KeepLast(10), Volatile; bloqueo máximo de escritura de un segundo.
Las muestras sintéticas usan las mismas fórmulas que RustDDS y salen a 1 Hz.

El IDL original contiene un miembro `sequence`, palabra reservada en IDL.
CMake genera en el build una entrada del compilador con un macro local que
escapa ese identificador como `_sequence`, y lo desactiva al terminar el archivo.
El mapeo C++ vuelve a llamarlo `sequence`; el orden, los tipos y la representación
CDR permanecen iguales. El archivo compartido no se modifica, y la entrada
generada se actualiza automáticamente cuando cambia. No editar los archivos
generados ni mantener otro IDL a mano.

| Puerto UDP local | Función |
| --- | --- |
| 7410 | Descubrimiento de participantes, SPDP |
| 7412 | Descubrimiento de extremos, SEDP |
| 7411 | Datos y confirmaciones RTPS/UDP |
| 3100 en 127.0.0.1 | Entrada NMEA del simulador, solo publicador |

La aplicación genera y elimina un INI temporal con las IP y puertos elegidos.
Usa `SpdpSendAddrs` hacia el par, `SedpMulticast=0` y `use_multicast=0` para datos.
No necesita DCPSInfoRepo ni `rtps_unicast_bridge.py`.

**Detalle de OpenDDS 3.34.0:** `UndirectedSpdp=0` también desactiva los anuncios
iniciales hacia `SpdpSendAddrs`. Por ello se mantiene `UndirectedSpdp=1` con
`TTL=0`: el multicast SPDP queda limitado al host; el intercambio entre las
máquinas usa UDP unicast. No se afirma que OpenDDS deje de crear sockets multicast
locales. Esto corrige la propuesta inicial de desactivar todo multicast.
`PeriodicDirectedSpdp=1` mantiene anuncios directos a participantes descubiertos.
No se abre multicast entre máquinas en el firewall.

Ejecutar las pruebas OpenDDS y RustDDS por separado: comparten dominio y algunos
puertos. No hay traducción de NAT ni DDS Security en este demo.

## Preparar OpenDDS en cada Linux

Trabajar dentro del sistema de archivos Linux, por ejemplo `~/projects/nmea-dds-demo`,
y conservar instalaciones y compilaciones separadas en WSL y la MV. No copiar
binarios Windows, bibliotecas ni cachés CMake entre máquinas. No usar sudo para
compilar ni ejecutar el demo.

Se necesita compilador C++17, CMake >= 3.20, Make, Perl y OpenDDS **3.34.0**;
Python 3 se usa para diagnóstico e integración. Comprobar herramientas:

```bash
cmake --version
perl --version
g++ --version
make --version
python3 --version
```

Si faltan, instalar en ese Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake perl git curl ca-certificates python3 unzip
```

Si OpenDDS ya está compilado, reutilizarlo. Descargar sus fuentes no basta:
deben existir `setenv.sh`, `bin/opendds_idl`, `ACE_wrappers/bin/tao_idl`,
`cmake/OpenDDSConfig.cmake` y las bibliotecas `.so` de OpenDDS/ACE/TAO.
Solo cuando falte una instalación compilada:

```bash
mkdir -p "$HOME/deps"
cd "$HOME/deps"
curl -fL https://github.com/OpenDDS/OpenDDS/releases/download/v3.34.0/OpenDDS-3.34.0.tar.gz -o OpenDDS-3.34.0.tar.gz
tar -xzf OpenDDS-3.34.0.tar.gz
cd OpenDDS-3.34.0
./configure --no-tests
make -j4
```

`configure` usa Perl/MPC y descarga ACE/TAO; requiere Internet. No mover la
instalación después de configurarla: `setenv.sh` y el paquete CMake contienen rutas.

## Compilar y ejecutar desde Bash

Desde la raíz del repositorio, tanto en WSL como en la MV:

```bash
bash opendds/scripts/build.sh --test
```

La instalación se selecciona en este orden: `--dds-root`, `MARINE_DDS_ROOT`,
`DDS_ROOT`, `$HOME/OpenDDS-3.34.0`. El script carga `setenv.sh`, comprueba versión,
generadores y dependencias Linux, configura CMake y compila. No modifica OpenDDS.
Si la instalación está en otro lugar, definirla en cada terminal:

```bash
export MARINE_DDS_ROOT="$HOME/deps/OpenDDS-3.34.0"
bash opendds/scripts/build.sh --test
```

`--dds-root` selecciona la instalación para esa compilación; para ejecutar con
la misma instalación usar `MARINE_DDS_ROOT` o cargar su `setenv.sh`.
El lanzador también carga el entorno automáticamente.

Valores predeterminados: `opendds/build-linux`, `Debug`, cuatro trabajos y pruebas
CTest desactivadas hasta solicitar `--test`. Ejemplo personalizado:

```bash
export MARINE_DDS_BUILD_DIR="opendds/build-local debug"
bash opendds/scripts/build.sh --build-dir "$MARINE_DDS_BUILD_DIR" --build-type Debug --jobs 4 --integration-tests
```

`--integration-tests` habilita y ejecuta CTest completo, incluida la prueba UDP local;
requiere Python 3 y sockets funcionales. Una compilación posterior sin esa opción
desactiva la integración en CTest. También se admiten Release, RelWithDebInfo y
MinSizeRel. Las rutas relativas de build se resuelven desde la raíz del repositorio,
incluso si se llama al script por ruta absoluta desde otra carpeta. Si cambia el
generador o la instalación OpenDDS, usar un directorio de build nuevo.
Los tipos generados permanecen en el build, ignorado por Git por defecto.

## WSL mirrored, VMware Bridged y firewall

En Windows, comprobar `wsl --version` y `wsl --status`. Mirrored requiere
Windows 11 22H2 o superior y una versión de WSL compatible. En
`%UserProfile%\.wslconfig`, conservar las otras opciones y establecer:

```ini
[wsl2]
networkingMode=mirrored
```

Guardar el trabajo de las distribuciones antes de ejecutar `wsl --shutdown` en
PowerShell; después abrir WSL de nuevo. Configurar VMware en **Bridged** sobre
la interfaz activa. Confirmar IP y ruta en ambos Linux:

```bash
ip -br -4 addr
ip route
```

La IP de `--local` debe pertenecer al Linux donde corre el proceso. No usar una
IP NAT antigua o deducirla exclusivamente de `ipconfig`. No ejecutar RustDDS y
OpenDDS simultáneamente: comparten dominio y puertos.

En PowerShell Windows **como administrador**, crear reglas dedicadas a OpenDDS
sin reemplazar las reglas existentes de RustDDS. Si ya existen, comprobarlas y
actualizar su origen cuando cambie la MV:

```powershell
New-NetFirewallHyperVRule `
  -Name 'NMEA-OpenDDS-WSL-Unicast' `
  -DisplayName 'NMEA OpenDDS - VMware hacia WSL' `
  -Direction Inbound `
  -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
  -Protocol UDP -LocalPorts '7410-7412' `
  -RemoteAddresses '192.168.10.33' -Action Allow

New-NetFirewallRule `
  -Name 'NMEA-OpenDDS-WSL-Windows' `
  -DisplayName 'NMEA OpenDDS - UDP desde VMware' `
  -Direction Inbound -Protocol UDP -LocalPort '7410-7412' `
  -RemoteAddress '192.168.10.33' -Profile Any -Action Allow
```

Si UFW está instalado y activo en la MV:

```bash
sudo ufw allow from 192.168.10.15 to any port 7410:7412 proto udp
```

Si está activo en WSL, autorizar análogamente el origen de la MV. No instalar UFW
solo para esta prueba ni desactivar firewalls. Una política de salida restrictiva
debe permitir los mismos puertos hacia el par. Los puertos `7410–7411` de RustDDS
no cubren SEDP en `7412`.

Antes de DDS, probar retorno UDP. En la MV:

```bash
python3 tools/udp_probe.py listen --local 192.168.10.33
```

En WSL:

```bash
python3 tools/udp_probe.py send --local 192.168.10.15 --peer 192.168.10.33
```

El probe usa UDP `17400`: necesita autorización separada en Windows/Hyper-V y
los Linux con firewall activo, limitada al par. Reutilizar las reglas previas de
ese puerto si corresponden a las IP actuales. Su éxito no verifica los puertos DDS.

Si faltan esas reglas, en PowerShell administrador:

```powershell
New-NetFirewallHyperVRule `
  -Name 'NMEA-OpenDDS-WSL-Probe' -DisplayName 'OpenDDS probe VMware hacia WSL' `
  -Direction Inbound -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
  -Protocol UDP -LocalPorts 17400 -RemoteAddresses '192.168.10.33' -Action Allow
New-NetFirewallRule `
  -Name 'NMEA-OpenDDS-WSL-Probe-Windows' -DisplayName 'OpenDDS probe UDP' `
  -Direction Inbound -Protocol UDP -LocalPort 17400 `
  -RemoteAddress '192.168.10.33' -Profile Any -Action Allow
```

Si UFW está activo en la MV: `sudo ufw allow from 192.168.10.15 to any port 17400 proto udp`.
En WSL con UFW activo, usar el origen `192.168.10.33` para esa misma regla.

## Ejecutar WSL → MV

Primero en VMware, desde la raíz del repositorio:

```bash
bash run.sh opendds subscriber --local 192.168.10.33 --peer 192.168.10.15
```

Después en WSL:

```bash
bash run.sh opendds publisher --local 192.168.10.15 --peer 192.168.10.33 --source synthetic
```

`PUBLICADO` confirma aceptación local. `RECIBIDO DDS` en la MV demuestra entrega.
Esperar hasta 30 segundos para la asociación y mantener la recepción un minuto.
La secuencia inicial puede ser mayor que uno por la durabilidad volátil.
Ctrl+C termina el proceso; comprobar que se puede reiniciar.

## Simulador Windows → WSL → MV

Configurar el simulador Windows para enviar **UDP** a `127.0.0.1:3100`. En WSL,
terminar el publicador sintético y ejecutar:

```bash
bash run.sh opendds publisher --local 192.168.10.15 --peer 192.168.10.33 \
  --source nmea --nmea-listen 127.0.0.1:3100
```

Comprobar con el simulador real que la consola WSL muestra la sentencia recibida
y que la MV recibe la misma `raw_nmea` y sus campos. Loopback DDS entre procesos
Linux no demuestra el recorrido UDP Windows→WSL. La secuencia avanza únicamente
con GPRMC válidas; un checksum incorrecto debe rechazarse. GPRMC no proporciona
heading ni profundidad; ambos se muestran como no disponibles.

Si no llega UDP por localhost, capturar en WSL:

```bash
sudo tcpdump -ni any 'udp port 3100'
```

Usar como destino del simulador una IPv4 de WSL comprobada con `ip -br -4 addr`
y alcanzable desde Windows. Escuchar en `--nmea-listen 0.0.0.0:3100`. Si se usa la
IP reflejada del anfitrión, comprobar la necesidad de `hostAddressLoopback=true`
bajo `[experimental]` en `.wslconfig`, reiniciando WSL tras cambiarlo. Confirmar
la entrega UDP en esa topología antes de continuar con DDS.

Para este acceso por dirección, autorizar **solo UDP 3100 desde la dirección de
origen real de Windows**. En PowerShell administrador, sustituir el ejemplo:

```powershell
$simulatorSource = '192.168.10.15'
New-NetFirewallHyperVRule `
  -Name 'NMEA-OpenDDS-WSL-NMEA' -DisplayName 'NMEA simulador hacia WSL' `
  -Direction Inbound -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
  -Protocol UDP -LocalPorts 3100 -RemoteAddresses $simulatorSource -Action Allow
New-NetFirewallRule `
  -Name 'NMEA-OpenDDS-WSL-NMEA-Windows' -DisplayName 'NMEA UDP hacia WSL' `
  -Direction Inbound -Protocol UDP -LocalPort 3100 `
  -RemoteAddress $simulatorSource -Profile Any -Action Allow
```

Si UFW está activo en WSL, permitir ese mismo origen hacia UDP 3100. Estas reglas
son distintas de DDS y del probe. No usar `netsh portproxy` para redirigir UDP.

## Pruebas y aceptación

```bash
bash opendds/scripts/build.sh --integration-tests
```

CTest `sources` verifica GPRMC y configuración; `cdr` verifica todos los campos del
tipo generado, ambas endianidades, cadenas y límites numéricos. `loopback` comprueba
ambos órdenes de arranque, reinicio del lector, sintético, NMEA, rechazo sin consumo
de secuencia, señales, IP inválida, puertos ocupados y reutilización tras el cierre.

Para probar dos procesos manualmente en el mismo Linux, usar puertos distintos:

```bash
bash run.sh opendds subscriber --local 127.0.0.1 --peer 127.0.0.1 \
  --spdp-port 17510 --sedp-port 17512 --data-port 17511 --peer-spdp-port 17410 --duration 15
bash run.sh opendds publisher --local 127.0.0.1 --peer 127.0.0.1 \
  --spdp-port 17410 --sedp-port 17412 --data-port 17411 --peer-spdp-port 17510 --duration 12
```

Aceptación entre máquinas: compilar y pasar CTest en WSL y la MV; recibir datos
sintéticos durante un minuto sin saltos posteriores al arranque; invertir el orden
de inicio; reiniciar el suscriptor; comparar GPRMC original y campos; inyectar un
checksum incorrecto; cerrar y reiniciar ambos procesos. Registrar versiones,
arquitectura, IP, comandos y logs. No confundir pruebas locales con esta aceptación.

## Diagnóstico y evidencia

En cada Linux:

```bash
bash run.sh opendds publisher --local 192.168.10.15 --peer 192.168.10.33 --debug 4 --duration 30
ss -lunp
ldd opendds/build-linux/publisher
sudo tcpdump -ni any 'udp and host 192.168.10.33 and portrange 7410-7412'
```

En la MV, usar el rol subscriber e intercambiar las IP del ejemplo. Si falta una
biblioteca, comprobar la instalación elegida y `setenv.sh`. Sin asociación,
revisar IP, rutas, firewall y SPDP/SEDP. Con asociación sin datos, revisar la fuente
UDP y los errores de escritura. Guardar ambas consolas y capturas en `logs/` y
`captures/`, ignorados por Git.

Consultar [VALIDATION.md](VALIDATION.md) para distinguir evidencia histórica,
validación WSL actual y pruebas pendientes entre máquinas.

Referencias oficiales: [compilación OpenDDS](https://opendds.readthedocs.io/en/latest-release/devguide/building/index.html),
[integración CMake](https://opendds.readthedocs.io/en/latest-release/devguide/building/cmake.html),
[configuración RTPS](https://opendds.readthedocs.io/en/latest-release/devguide/run_time_configuration.html),
[red WSL y firewall](https://learn.microsoft.com/en-us/windows/wsl/networking),
[opciones .wslconfig](https://learn.microsoft.com/en-us/windows/wsl/wsl-config).
