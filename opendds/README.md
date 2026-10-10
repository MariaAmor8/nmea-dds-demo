# Guía OpenDDS: Ubuntu MV → Ubuntu MV

Implementación C++ del [demo común](../README.md), con OpenDDS **3.34.0**. Seguir los pasos en ambas MV salvo que se indique un rol. Se parte del clon en `~/projects/nmea-dds-demo`; para descargarlo seguir el README raíz.

## 1. Instalar herramientas (ambas MV)

```bash
sudo apt update
sudo apt install -y build-essential cmake make perl git curl ca-certificates python3
cmake --version
g++ --version
make --version
perl --version
python3 --version
```

Resultado esperado: versiones disponibles, CMake **≥3.20** y compilador con C++17. Python se utiliza en diagnóstico y pruebas de integración. Compilar y ejecutar como usuario normal.

## 2. Instalar OpenDDS 3.34.0 (ambas MV)

Si existe una instalación compilada de esta versión, reutilizarla. En caso contrario:

```bash
mkdir -p "$HOME/deps"
cd "$HOME/deps"
curl -fL https://github.com/OpenDDS/OpenDDS/releases/download/v3.34.0/OpenDDS-3.34.0.tar.gz -o OpenDDS-3.34.0.tar.gz
tar -xzf OpenDDS-3.34.0.tar.gz
cd OpenDDS-3.34.0
./configure --no-tests
make -j4
```

`configure` prepara el build y descarga ACE/TAO; requiere Internet. `--no-tests` omite las pruebas de la biblioteca, no las del demo. `make` debe terminar sin errores; reducir a `-j2` si falta memoria. No mover la instalación después de configurarla porque el entorno contiene rutas. Procedimiento basado en la [guía oficial de compilación](https://opendds.readthedocs.io/en/latest-release/devguide/building/index.html) y la [publicación 3.34.0](https://github.com/OpenDDS/OpenDDS/releases/tag/v3.34.0).

Comprobar los artefactos principales:

```bash
ls setenv.sh cmake/OpenDDSConfig.cmake bin/opendds_idl ACE_wrappers/bin/tao_idl
ls lib/libOpenDDS_Dcps.so lib/libOpenDDS_Rtps.so lib/libOpenDDS_Rtps_Udp.so
```

Deben existir todos; descargar las fuentes no basta. El script del proyecto también verifica versión y dependencias de los generadores.

## 3. Seleccionar entorno y compilar (ambas MV)

En **cada terminal** utilizada para compilar o ejecutar:

```bash
export MARINE_DDS_ROOT="$HOME/deps/OpenDDS-3.34.0"
cd "$HOME/projects/nmea-dds-demo"
bash opendds/scripts/build.sh --test
```

Si se reutiliza otra instalación, sustituir la ruta. El script carga automáticamente su `setenv.sh`, configura CMake, genera tipos C++ y compila. El lanzador `run.sh` también carga el entorno.

Selección de instalación, en orden: `--dds-root` del script de build, `MARINE_DDS_ROOT`, `DDS_ROOT`, `$HOME/OpenDDS-3.34.0`. `--dds-root` solo afecta esa compilación; para ejecutar con la misma instalación, exportar `MARINE_DDS_ROOT` o cargar su `setenv.sh` en la terminal.

Resultado esperado: publicador y suscriptor compilados en `opendds/build-linux/`, y CTest `sources` y `cdr` aprobados. CMake adapta automáticamente el identificador IDL `sequence` en su entrada de compilador; no editar el IDL común ni los tipos generados.

| Opción del build | Predeterminado | Función |
| --- | --- | --- |
| `--dds-root RUTA` | Precedencia anterior | Seleccionar instalación OpenDDS |
| `--build-dir RUTA` | `MARINE_DDS_BUILD_DIR` o `opendds/build-linux` | Directorio de compilación |
| `--build-type TIPO` | `Debug` | `Debug`, `Release`, `RelWithDebInfo` o `MinSizeRel` |
| `--jobs N` | `4` | Trabajos paralelos; entero positivo |
| `--test` | Pruebas no ejecutadas | Ejecutar pruebas CTest sin habilitar integración |
| `--integration-tests` | Desactivada | Habilitar y ejecutar CTest completo con integración local |
| `--help`, `-h` | — | Mostrar ayuda |

Ejemplo opcional de build personalizado:

```bash
export MARINE_DDS_BUILD_DIR="opendds/build-release"
bash opendds/scripts/build.sh --build-dir "$MARINE_DDS_BUILD_DIR" --build-type Release --jobs 4 --integration-tests
```

Resultado esperado: compilación y tests `sources`, `cdr`, `multicast` aprobados. CTest ejecuta `opendds/tests/integration.py`, que arranca los ejecutables y revisa sus logs; las muestras DDS no pasan por el script. La integración verifica entrega y reasociación entre procesos locales y entrada NMEA; necesita sockets funcionales y no valida las dos MV. Ejecutarla sin otros demos activos. Un build posterior sin `--integration-tests` deshabilita esa prueba.

Las rutas relativas se resuelven desde la raíz del repositorio. Para ejecutar un build personalizado, definir `MARINE_DDS_BUILD_DIR` con la misma ruta; `--build-dir` no exporta esa variable. Al cambiar instalación o generador CMake, usar un directorio de build nuevo.

## 4. Preparar red (ambas MV)

Seguir [pasos 1–3 de MULTICAST.md](../MULTICAST.md): VMware Bridged y selección explícita del adaptador físico de Internet. En cada MV comprobar:

```bash
ip -br -4 addr
ip route
ip link
```

Cada MV necesita una dirección distinta y conectividad multicast. Las IP se consultan para diagnóstico y firewall; no se pasan al demo. OpenDDS y el sistema gestionan las interfaces automáticamente. La opción antigua `--local IP` se rechaza. Varias interfaces, VPN o rutas inadecuadas pueden impedir conectividad.

| Protocolo/puerto local | Función |
| --- | --- |
| UDP `239.255.0.1:7400` | SPDP multicast, TTL 1 |
| UDP `7410` | SPDP unicast |
| UDP `7412` | SEDP, descubrimiento de extremos |
| UDP `7411` | Datos RTPS y control/confirmaciones |
| UDP `127.0.0.1:3100` | NMEA opcional, solo publicador |

La aplicación crea y elimina un INI temporal: deja la interfaz multicast sin selección explícita y escucha SPDP, SEDP y datos en `0.0.0.0:PUERTO`, usa SPDP multicast y SEDP/datos unicast. `0.0.0.0` significa escucha general; no es una IP remota ni se anuncia como dirección de destino. No necesita DCPSInfoRepo ni auxiliar Python. Aplicar las reglas UFW de la guía de red en ambas MV si el firewall está activo; Reliable requiere retorno.

## 5. Ejecutar la fuente sintética

Primero, **MV B suscriptora**, con el entorno definido en esa terminal:

```bash
cd "$HOME/projects/nmea-dds-demo"
bash run.sh opendds subscriber
```

Resultado esperado: descripción de dominio/tópico, línea `SPDP multicast 239.255.0.1:7400 TTL=1` con el mensaje `interfaces automaticas (OpenDDS/sistema)` y los puertos, y `Suscriptor listo. Esperando MarineNavigation. Ctrl+C para terminar.`

Después, **MV A publicadora**, con su propio entorno:

```bash
cd "$HOME/projects/nmea-dds-demo"
bash run.sh opendds publisher --source synthetic
```

Resultado esperado: `Publicador listo. Fuente: synthetic. Ctrl+C para terminar.`, muestras aproximadamente a 1 Hz y asociación:

```text
PublicationMatched lectores=1 total=1
```

El suscriptor informa:

```text
SubscriptionMatched escritores=1 total=1
RECIBIDO DDS:
PARSEADO: seq=12 timestamp_ms=1791637200000 lat=10.00012 lon=-75.00012 speed=5.2 kn course=12.0 heading=14.0 depth=8.4 m position_valid=true heading_valid=true depth_valid=true simulated=true
```

El publicador imprime `PUBLICADO DDS:` seguido del mismo resumen de campos. El texto NMEA está vacío en modo sintético. Publicar confirma aceptación local; recibir en la otra MV confirma entrega. La primera secuencia puede ser mayor que uno por el tiempo de asociación y la durabilidad Volatile; los saltos posteriores aparecen como `SALTO secuencia`.

Esperar hasta 30 segundos antes de diagnosticar asociación. Para una comprobación nueva, observar un minuto; esta duración no fue confirmada en la prueba reportada. Detener ambos procesos con Ctrl+C e **invertir roles**: subscriber en MV A, publisher en MV B, sin proporcionar IP en ninguno de los comandos. El usuario reportó recepción sintética exitosa en ambos sentidos con la configuración anterior de IP explícita. La aceptación entre MV del nuevo modo automático está pendiente.

## 6. Opciones de ejecución y NMEA

```bash
bash run.sh opendds publisher --help
bash run.sh opendds subscriber --help
# Ejecución sintética limitada, en la MV publicadora:
bash run.sh opendds publisher --source synthetic --duration 65
```

El último comando termina automáticamente tras unos 65 segundos de su bucle de ejecución con `Publicador terminado.`; no cierra el suscriptor remoto.

| Opción | Predeterminado | Función |
| --- | --- | --- |
| `--duration SEGUNDOS` | `0` | `0`: hasta Ctrl+C; `1–86400`: duración del bucle, ambos roles |
| `--debug NIVEL` | `0` | Depuración OpenDDS, `0–10`, ambos roles |
| `--spdp-port N` | `7410` | Puerto SPDP unicast local |
| `--sedp-port N` | `7412` | Puerto SEDP local |
| `--data-port N` | `7411` | Puerto local de datos/control |
| `--source synthetic\|nmea` | `synthetic` | Fuente; solo publicador |
| `--nmea-listen IP:PUERTO` | `127.0.0.1:3100` | Escucha NMEA; solo publicador |
| `--help` | — | Usar sola para mostrar ayuda y salir |

Los tres puertos DDS personalizados deben ser distintos y estar entre 1 y 65535. No cambian el destino multicast SPDP `239.255.0.1:7400`; actualizar reglas y capturas para los nuevos puertos. Ejemplo de diagnóstico: añadir `--debug 4 --duration 30` al comando del rol correspondiente.

NMEA está disponible pero **no se validó entre las MV en la prueba reportada**. El publicador admite `--source nmea --nmea-listen IP:PUERTO`, conserva `raw_nmea`, valida checksum GPRMC y rechaza líneas inválidas. El endpoint predeterminado recibe únicamente desde esa MV. GPRMC no contiene heading/profundidad: se muestran `N/D`, con `simulated=false`. Esta guía no incluye un recorrido de simulador.

## 7. Diagnóstico

Consultar la [tabla de síntomas y capturas](../MULTICAST.md). En la raíz:

```bash
ss -lunp
source "$MARINE_DDS_ROOT/setenv.sh"
ldd "${MARINE_DDS_BUILD_DIR:-opendds/build-linux}/publisher"
```

Resultado esperado: puertos locales del rol activo y ninguna biblioteca `not found`. Si faltan bibliotecas, revisar instalación y entorno; si se rechaza `--local`, retirar esa opción del comando. Sin asociación, revisar SPDP/SEDP, Bridged y firewall. Con asociación sin muestras, revisar fuente, errores de escritura y retorno. Guardar salidas de ambas consolas en `logs/`.

Para la función y el momento de ejecución de todos los scripts, consultar la [tabla de scripts Python](../README.md#scripts-python-y-momento-de-ejecución). `tools/udp_probe.py` y `tools/capture_spdp.py` son diagnósticos manuales opcionales descritos en [MULTICAST.md](../MULTICAST.md). `tools/tests/test_capture_spdp.py` prueba el observador con paquetes simulados, sin iniciar DDS. `tools/idl_to_rust.py` pertenece exclusivamente a la compilación Rust; OpenDDS genera sus tipos con sus propias herramientas.
