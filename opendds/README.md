# Demo OpenDDS 3.34.0

Publicador C++ en **Windows nativo** `192.168.10.15` y suscriptor C++ en
**Ubuntu VMware** `192.168.10.33`. Ambos usan `../idl/Navigation.idl`, sin copia
manual del tipo DDS. RustDDS permanece independiente y se ejecuta en WSL.

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

## Windows: reutilizar la instalación existente

Se inspeccionó `C:\OpenDDS-3.34.0\OpenDDS-3.34.0`: OpenDDS 3.34.0, ACE 6.5.24,
TAO 2.5.24, DLL **x64 Debug**, solución Visual Studio 2022, compilador `cl`.
`setenv.cmd` y `cmake/config.cmake` contienen rutas antiguas de Downloads.
El script de compilación adapta una copia privada del paquete CMake dentro del
build y configura PATH con la ubicación actual; no modifica la instalación.

Desde PowerShell Windows, en la raíz de una copia del repositorio:

```powershell
.\opendds\scripts\build.cmd -Test
```

El script importa las herramientas MSVC 2022 x64, encuentra CMake (incluido el de
Visual Studio si no está en PATH), genera el IDL, compila y ejecuta CTest.
No requiere ejecutarse como administrador. Para otra ubicación o configuración:

```powershell
.\opendds\scripts\build.cmd -DdsRoot 'C:\ruta\OpenDDS-3.34.0' -Configuration Debug -Test
$env:MARINE_DDS_ROOT = 'C:\ruta\OpenDDS-3.34.0'
$env:MARINE_DDS_CONFIG = 'Debug'
```

El valor predeterminado es Debug porque las DLL disponibles son Debug. Release
requiere primero bibliotecas OpenDDS/ACE/TAO Release x64. El script comprueba
arquitectura y dependencias. Si se cambia de generador/arquitectura, utilizar otro
`-BuildDir`; para ejecutarlo, definir `MARINE_DDS_BUILD_DIR` con esa misma ruta.
Se recomienda una copia nativa Windows del repositorio para el uso diario: MSBuild
puede producir advertencias de dependencias al compilar sobre el sistema de archivos
WSL, que distingue mayúsculas y minúsculas.

## Windows: instalar desde cero con Perl, MSVC y CMake

1. Instalar Visual Studio 2022 o Build Tools 2022, con **Desktop development with
   C++**, herramientas MSVC x64, Windows SDK y herramientas CMake. VS Code es un
   editor; no proporciona el compilador MSVC por sí mismo.
2. Instalar [Strawberry Perl](https://strawberryperl.com/), CMake si no se instaló
   con Visual Studio, y Git para descargar dependencias y clonar este repositorio.
3. Descargar el archivo de fuentes de la [versión OpenDDS 3.34.0](https://github.com/OpenDDS/OpenDDS/releases/tag/v3.34.0)
   y extraerlo, por ejemplo, en `C:\OpenDDS-3.34.0\OpenDDS-3.34.0`. El directorio
   elegido debe contener `configure`, `VERSION.txt` y `dds/`.
4. Abrir **x64 Native Tools Command Prompt for VS 2022** y ejecutar:

```bat
cd /d C:\OpenDDS-3.34.0\OpenDDS-3.34.0
perl --version
cl
cmake --version
configure
msbuild DDS_TAOv2.sln -m:4 -p:Configuration=Debug,Platform=x64
call setenv.cmd
```

`configure` usa Perl/MPC y descarga/configura ACE/TAO; requiere acceso a Internet.
La solución y el nombre indicado por `configure` son la referencia si difieren de
los del ejemplo. Comprobar `lib\OpenDDS_Dcpsd.dll`, `OpenDDS_Rtpsd.dll`,
`OpenDDS_Rtps_Udpd.dll`, `ACE_wrappers\lib\ACEd.dll`, `TAOd.dll` y
`bin\opendds_idl.exe`. Después compilar **el demo** con `build.cmd -Test`.
Esta ruta usa Perl/MPC para preparar OpenDDS y CMake para construir el demo;
no mezcla bibliotecas de instalaciones diferentes. No mover la instalación una
vez configurada salvo que se gestione la actualización de sus rutas.

## Ubuntu VMware: instalar y compilar

Se requiere un compilador C++17 y CMake 3.20 o superior. En Ubuntu, desde una terminal normal:

```bash
sudo apt update
sudo apt install -y build-essential cmake perl git curl ca-certificates python3 unzip
mkdir -p "$HOME/deps"
cd "$HOME/deps"
curl -fL https://github.com/OpenDDS/OpenDDS/releases/download/v3.34.0/OpenDDS-3.34.0.tar.gz -o OpenDDS-3.34.0.tar.gz
tar -xzf OpenDDS-3.34.0.tar.gz
cd OpenDDS-3.34.0
./configure --no-tests
make -j4
source setenv.sh
```

El `configure` de esta versión descarga ACE/TAO y MPC. No reemplazar sus
bibliotecas por las de Windows ni copiar binarios `.exe` a Ubuntu. Si ya hay una
instalación Linux de OpenDDS 3.34.0, cargar su `setenv.sh` y omitir la instalación.
Usar el nombre de directorio realmente creado por el archivo descargado.

En la raíz del repositorio clonado o copiado a Ubuntu:

```bash
cmake -S opendds -B opendds/build-linux \
  -DOpenDDS_DIR="$DDS_ROOT/cmake" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build opendds/build-linux --parallel 4
ctest --test-dir opendds/build-linux --output-on-failure
```

CMake exige **exactamente 3.34.0**. Los tipos generados y binarios quedan en
`opendds/build-linux/`, excluido de Git. Cargar `source "$HOME/deps/OpenDDS-3.34.0/setenv.sh"`
en cada terminal nueva antes de ejecutar. No se necesita Rust para compilar
OpenDDS. No usar sudo para compilar o ejecutar el demo.

## Firewall y conectividad

Confirmar `192.168.10.15` con `ipconfig` en Windows y `192.168.10.33` con
`ip -br -4 addr` en VMware. Mantener VMware Bridged. OpenDDS Windows no depende
de la configuración mirrored de WSL ni de sus reglas Hyper-V.

En PowerShell Windows **como administrador**, una vez:

```powershell
New-NetFirewallRule `
  -Name 'NMEA-OpenDDS-Windows-Unicast' `
  -DisplayName 'NMEA OpenDDS - UDP desde VMware' `
  -Direction Inbound -Protocol UDP -LocalPort '7410-7412' `
  -LocalAddress '192.168.10.15' -RemoteAddress '192.168.10.33' `
  -Profile Any -Action Allow
```

Si UFW está instalado y activo en VMware:

```bash
sudo ufw allow from 192.168.10.15 to any port 7410:7412 proto udp
```

Las políticas de salida deben permitir los mismos puertos hacia el par; si existe
una política restrictiva, autorizar ese destino. No desactivar el firewall.
Si cambian las IP, actualizar comandos y reglas. Las reglas RustDDS para
`7410–7411` no cubren SEDP en `7412` ni necesariamente el proceso Windows.

El probe existente puede comprobar el retorno UDP antes de DDS. En VMware:

```bash
python3 tools/udp_probe.py listen --local 192.168.10.33
```

Y desde Windows con Python 3 instalado, en la raíz del repositorio:

```powershell
py -3 tools\udp_probe.py send --local 192.168.10.15 --peer 192.168.10.33
```

Ese probe usa UDP 17400, que requiere su propia autorización de firewall;
no demuestra que los puertos DDS estén abiertos.

## Ejecutar el demo

Primero en VMware, tras cargar el entorno Linux:

```bash
bash run.sh opendds subscriber --local 192.168.10.33 --peer 192.168.10.15
```

Después en PowerShell Windows, desde la raíz del repo:

```powershell
.\opendds\run.ps1 publisher --local 192.168.10.15 --peer 192.168.10.33 --source synthetic
```

Para recibir GPRMC del simulador Windows, terminar el publicador y ejecutar:

```powershell
.\opendds\run.ps1 publisher --local 192.168.10.15 --peer 192.168.10.33 --source nmea --nmea-listen 127.0.0.1:3100
```

Configurar el simulador para enviar a `127.0.0.1:3100` en el mismo Windows.
La entrada NMEA ya no pasa por WSL en esta implementación.

Para una prueba sin simulador, después de ver `PublicationMatched lectores=1`,
enviar esta sentencia desde otra ventana PowerShell en Windows:

```powershell
$udp = [Net.Sockets.UdpClient]::new()
try {
  $line = 'GPS1 on UDP2: $GPRMC,131537.83,A,0000.95081,S,00000.29512,W,0010.0,089.0,051026,0.0,W,A,S*66'
  $bytes = [Text.Encoding]::ASCII.GetBytes($line + "`r`n")
  [void]$udp.Send($bytes, $bytes.Length, '127.0.0.1', 3100)
} finally { $udp.Dispose() }
```

Se acepta una sentencia `$GPRMC` con prefijo, como `GPS1 on UDP2:`. El checksum
XOR debe coincidir; cada datagrama puede contener varias líneas. `raw_nmea`
conserva toda la línea sin espacios exteriores. `course_deg` es curso sobre el
suelo, no rumbo verdadero; GPRMC no suministra heading ni profundidad, que se
publican como `0.0` y con indicadores `false`, y se imprimen `N/D`.
El estado `A` publica `position_valid=true`; `V` publica `false` si los demás
campos obligatorios son parseables. Campos vacíos/incorrectos se rechazan.

Para mantener la convención temporal Rust actual, `timestamp_ms` usa fecha UTC,
año `2000 + YY` y segundos enteros; la fracción horaria se valida pero no se
incorpora. OpenDDS valida además fechas civiles reales, números finitos y
velocidad no negativa. No se modifica el parser Rust. `simulated=false` identifica
la fuente UDP; no demuestra que el emisor sea un GPS físico.

La secuencia empieza en 1 y avanza solo para líneas aceptadas, aunque una escritura
DDS posterior falle; el error de escritura se informa. La fuente sintética utiliza
`simulated=true`. `PUBLICADO DDS` confirma aceptación local; **`RECIBIDO DDS` en
VMware es la evidencia de entrega**. PublicationMatched/SubscriptionMatched
confirman asociación, no recepción de todas las muestras.

La primera secuencia recibida puede ser mayor que 1 por la durabilidad volátil.
Tras reiniciar el publicador, el lector puede señalar un salto por el reinicio de
secuencia. No medir latencia con timestamps sin sincronización de relojes.
Ctrl+C cierra sockets y entidades DDS; `--duration 60` permite una prueba finita.

Si PowerShell bloquea scripts por su política local, se puede ejecutar el lanzador
con `powershell -NoProfile -ExecutionPolicy Bypass -File .\opendds\run.ps1 ...`;
esto se aplica a ese proceso, sin cambiar la política global.

## Pruebas reproducibles

`CTest` ejecuta pruebas de fuentes/configuración y un roundtrip CDR de todos los
campos del tipo generado, incluida la cadena, ambos órdenes de bytes, indicadores
true/false y los límites de los enteros. Las comprobaciones siguen activas en Release.

Para probar dos procesos en una sola máquina, asignar puertos locales distintos.
Ejemplo Linux en dos terminales con el entorno OpenDDS cargado:

```bash
bash run.sh opendds subscriber --local 127.0.0.1 --peer 127.0.0.1 \
  --spdp-port 17510 --sedp-port 17512 --data-port 17511 --peer-spdp-port 17410 --duration 15
bash run.sh opendds publisher --local 127.0.0.1 --peer 127.0.0.1 \
  --spdp-port 17410 --sedp-port 17412 --data-port 17411 --peer-spdp-port 17510 --duration 12
```

En Windows usar `opendds/run.ps1` con los mismos roles y argumentos. No lanzar
los dos procesos con los puertos predeterminados en el mismo host.

El test automatizado de integración necesita Python 3 y el entorno OpenDDS:

```bash
python3 opendds/tests/integration.py --bin-dir opendds/build-linux
```

En Windows, configurar PATH mediante el entorno de OpenDDS y ejecutar:

```powershell
$env:PATH = 'C:\OpenDDS-3.34.0\OpenDDS-3.34.0\lib;C:\OpenDDS-3.34.0\OpenDDS-3.34.0\ACE_wrappers\lib;' + $env:PATH
py -3 opendds\tests\integration.py --bin-dir opendds\build-windows\Debug
```

También puede habilitarse en CTest reconfigurando CMake con
`-DMARINE_INTEGRATION_TESTS=ON` (requiere Python 3 y sockets locales funcionales).

Comprueba ambos órdenes de arranque, reinicio del lector, transmisión sintética y
NMEA, rechazo sin consumo de secuencia, cierre por señal, IP inválida, puerto
ocupado y correspondencia exacta entre resúmenes publicados y recibidos. Los logs
se guardan en un directorio temporal y su ruta se imprime. Una prueba local no
sustituye la prueba Windows→VMware.

Aceptación en las dos máquinas del usuario:

1. Compilar y pasar CTest en Windows y Ubuntu; registrar versión, arquitectura y configuración.
2. Mantener recepción sintética un minuto; verificar asociación y ausencia de saltos después del arranque.
3. Enviar GPRMC y comparar la línea original y campos en las dos consolas; inyectar checksum incorrecto y comprobar rechazo.
4. Invertir orden de arranque; reiniciar el suscriptor; comprobar que vuelve a recibir muestras nuevas.
5. Probar IP local inexistente y puerto ocupado: deben producir error, no anunciar éxito de entrega.
6. Terminar con Ctrl+C y reiniciar, comprobando que los puertos quedan disponibles.

## Diagnóstico

Esperar hasta 30 segundos antes de diagnosticar asociación. Para más información:

```powershell
.\opendds\run.ps1 publisher --local 192.168.10.15 --peer 192.168.10.33 --debug 4
Get-NetUDPEndpoint | Where-Object LocalPort -in 7410,7411,7412
```

En VMware:

```bash
bash run.sh opendds subscriber --local 192.168.10.33 --peer 192.168.10.15 --debug 4
ss -lunp
sudo tcpdump -ni any 'udp and host 192.168.10.15 and portrange 7410-7412'
```

En Windows, Wireshark con filtro
`udp && ip.addr == 192.168.10.33 && udp.port >= 7410 && udp.port <= 7412`.
Sin asociación: revisar IP, rutas, firewall y paquetes SPDP/SEDP. Con asociación
pero sin datos: revisar fuente UDP y errores de escritura. Una IP remota válida
pero incorrecta deja al proceso esperando; `--duration` limita la prueba.
Errores de DLL: usar Debug/Release correspondiente y PATH de ACE/OpenDDS;
no mezclar x86 con x64. Guardar las dos consolas y la captura al reportar un fallo.

## Evidencia y límites

El estado de validación de esta entrega se registra en [VALIDATION.md](VALIDATION.md).
La comunicación entre Windows y VMware solo puede certificarse con ejecución en
esas máquinas. No se presenta una compilación o un roundtrip CDR como prueba de
comunicación entre hosts.

Referencias oficiales: [compilación OpenDDS](https://opendds.readthedocs.io/en/latest-release/devguide/building/index.html),
[integración CMake](https://opendds.readthedocs.io/en/latest-release/devguide/building/cmake.html),
[configuración RTPS](https://opendds.readthedocs.io/en/latest-release/devguide/run_time_configuration.html).
Los parámetros se contrastaron también con la documentación y el código fuente
incluidos en la instalación 3.34.0, en particular `dds/DCPS/RTPS/Spdp.cpp`.

### Bloqueo de DLL en Windows observado en este entorno

Los binarios MSVC se compilan, pero la política Windows de integridad/firma
bloquea `ACE_wrappers\lib\TAO_Valuetyped.dll` de la instalación existente antes
de entrar en el programa. El proceso devuelve `0xc0e90002`; CTest lo muestra
como excepción. Los eventos CodeIntegrity 3033 y 3077 identifican esa DLL.
Este bloqueo también afecta `publisher --help`, por lo que no es un fallo de red.

Para consultar los eventos relevantes, sin cambiar políticas:

```powershell
Get-WinEvent -FilterHashtable @{
  LogName='Microsoft-Windows-CodeIntegrity/Operational'
  StartTime=(Get-Date).AddMinutes(-15)
} | Where-Object Message -match 'OpenDDS|TAO_Valuetyped|publisher.exe|cdr_tests.exe' |
  Select-Object Id,Message | Format-List
```

La dependencia debe cumplir la política de firma/autorización de ese Windows,
con ayuda de quien administre dicha política. No se desactiva la protección ni
se cambia la instalación como parte de este demo. Una vez resuelto, repetir
`build.cmd -Test` y las pruebas de comunicación. El README no declara recepción
Windows→VMware como validada mientras exista este bloqueo.
