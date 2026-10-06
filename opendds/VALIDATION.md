# Validación de la implementación

Fecha: 6 de octubre de 2026. Este archivo distingue compilación, pruebas locales
y comunicación entre las máquinas del usuario.

| Comprobación | Resultado |
| --- | --- |
| Rust: `cargo test --locked --workspace` | 3 pruebas pasan |
| RustDDS, generador, auxiliar, dependencias e IDL compartido | Sin modificaciones |
| Fuentes/configuración/UDP C++: GCC, C++17, warnings activados | Compila; pruebas pasan |
| Windows: generación del IDL con OpenDDS 3.34.0 | Pasa; `_sequence` se mapea al campo C++ `sequence` |
| Windows: MSVC 19.44 / VS 2022, x64 Debug | Publicador, suscriptor y pruebas compilan |
| Windows: CTest `sources` | Pasa |
| Windows: CTest `cdr` y ejecución del publicador | Bloqueadas por CodeIntegrity, `0xc0e90002` |
| Linux: OpenDDS 3.34.0 y demo completo, CMake 3.31.6, C++17 | Compilan |
| Linux: CTest `sources` y `cdr` | Pasan; CDR verifica todos los campos, ambas endianidades y cadenas vacías/largas |
| Linux: CTest `loopback`, fuera del sandbox | Pasa en 23.37 s |
| Windows→Ubuntu VMware, sintético durante un minuto | Pendiente de ejecutar en las dos máquinas |
| Windows→Ubuntu VMware, simulador GPRMC | Pendiente de ejecutar en las dos máquinas |

La instalación Windows existente contiene DLL Debug x64 con ACE 6.5.24 y TAO
2.5.24. Sus rutas obsoletas se adaptan en una copia privada CMake del build;
la instalación original no se modifica.

Los eventos CodeIntegrity 3033/3077 muestran bloqueo de
`C:\OpenDDS-3.34.0\OpenDDS-3.34.0\ACE_wrappers\lib\TAO_Valuetyped.dll` por requisitos
de firma/política. La prueba CDR y `publisher --help` terminan antes de ejecutar
el código del demo. No se ha cambiado la política ni se ha eludido el bloqueo.
El administrador del Windows debe resolver la autorización de esa dependencia
antes de validar la ejecución nativa.

Ver [README.md](README.md) para repetir pruebas, diagnosticar el bloqueo y
registrar evidencia de recepción real en VMware.

## Evidencia Linux

OpenDDS se descargó de la publicación oficial `v3.34.0` y se compiló desde
fuentes en `/tmp/OpenDDS-3.34.0`, con ACE 6.5.24 y TAO 2.5.24. El demo se
compiló en `opendds/build-linux` con GCC 15.2.0 y CMake portable 3.31.6.
También se comprobó el lanzador Linux: detecta dependencias ausentes sin el
entorno cargado y permite ejecutar ambos roles con `--help` después de cargarlo.

La primera ejecución de loopback dentro del sandbox falló porque ese entorno
prohíbe abrir sockets Netlink y UDP. Se repitió únicamente la prueba de red
fuera del sandbox, con autorización, y pasó. Las pruebas de fuentes y CDR
ya habían pasado dentro del sandbox.

La integración verifica ambos órdenes de arranque, recepción sintética
continua, reinicio del lector manteniendo el publicador, NMEA con dos líneas
válidas y una inválida en un datagrama, identidad de `raw_nmea` y resúmenes,
secuencias 1/2 sin consumir número para la línea rechazada, cierre por SIGINT,
IP local inválida/inexistente, puertos NMEA/SPDP ocupados y reutilización de
los siete puertos después del cierre. Los puertos de esta prueba son locales,
distintos para cada participante; no utiliza las IP de Windows/VMware.

Logs de la ejecución que pasó: `/tmp/marine-opendds-tests-ierrwqwg/`. Son
artefactos temporales de esta máquina, no archivos versionados. Para repetir:

```bash
source /tmp/OpenDDS-3.34.0/setenv.sh
/tmp/cmake-3.31.6-linux-x86_64/bin/ctest --test-dir opendds/build-linux --output-on-failure
```

La prueba de red necesita permisos de sockets normales. En un entorno
restringido debe ejecutarse con autorización fuera del sandbox. Esta evidencia
local no certifica el firewall, la conectividad ni la recepción en VMware.
