# Guía RustDDS: Ubuntu MV → Ubuntu MV

Implementación Rust del [demo común](../README.md), con RustDDS **0.14.3 original de crates.io**, sin parche local. Ejecutar estos pasos en ambas MV salvo que se indique un rol. Se parte del clon en `~/projects/nmea-dds-demo`; para descargarlo seguir el README raíz.

## 1. Instalar y comprobar herramientas (ambas MV)

```bash
sudo apt update
sudo apt install -y build-essential pkg-config python3 git curl ca-certificates
```

Si Rust/Cargo todavía no están instalados, usar [rustup oficial](https://rust-lang.org/tools/install/) y elegir la instalación estándar:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
```

Comprobar:

```bash
rustc --version
cargo --version
python3 --version
cc --version
```

Resultado esperado: todas las herramientas imprimen su versión; Rust/Cargo deben ser **1.88.0 o posteriores**, mínimo requerido por RustDDS y las dependencias resueltas para Ubuntu x86_64. Si Rust es anterior, ejecutar `rustup update stable`. Si Cargo no se encuentra, cargar `"$HOME/.cargo/env"` en esa terminal. `rustdds/build.rs` ejecuta automáticamente `tools/idl_to_rust.py` para generar los tipos desde el IDL durante la compilación cuando corresponde; Python no es opcional. Cargo.lock fija dependencias y los comandos usan `--locked`.

## 2. Preparar la red (ambas MV)

Seguir [pasos 1–3 de MULTICAST.md](../MULTICAST.md): VMware Bridged con adaptador físico explícito, IPv4 distintas, interfaz activa con multicast y reglas UFW cuando esté activo.

| Protocolo/puerto | Función |
| --- | --- |
| UDP `239.255.0.1:7400` | Descubrimiento SPDP |
| UDP multicast `239.255.0.1:7401` | Socket de recepción de datos multicast nativo |
| UDP unicast `7410/7411` | Descubrimiento y datos/control para participante 0 |
| UDP `127.0.0.1:3100` | Entrada NMEA opcional, no usada en la prueba sintética |

Los puertos dependen del ID y pueden variar si están ocupados; consultar la guía de red. RustDDS enumera interfaces automáticamente: **no admite `--local` ni una IP remota**. No se necesitan variables de IP para ejecutar el demo. Las variables de la guía de red sirven solo para diagnóstico/firewall.

## 3. Compilar y probar (ambas MV)

```bash
cd "$HOME/projects/nmea-dds-demo"
cargo build --locked -p marine-rustdds --bins
cargo test --locked -p marine-rustdds
```

Resultado esperado: compilación finalizada sin errores y pruebas GPRMC/checksum y CDR aprobadas. La prueba CDR utiliza la representación anunciada por el serializador y comprueba todos los campos, texto NMEA/UTF-8 y las 16 combinaciones de booleanos. Los ejecutables quedan en `target/debug/`; los tipos generados quedan en `target/`. No copiar artefactos entre MV.

Opcionalmente, sin otros demos activos:

```bash
python3 rustdds/tests/multicast.py
```

`rustdds/tests/multicast.py` arranca los ejecutables y revisa sus logs; las muestras DDS no pasan por el script. La integración local verifica asociación, ambos órdenes de inicio, entrega, reasociación y NMEA válido/inválido. Debe terminar con código cero. Ejecuta procesos en la misma MV y no valida la red entre MV.

**Validación local de RustDDS 0.14.3 (10 de octubre de 2026):** compilación de ambos binarios con `--locked`, las tres pruebas NMEA/CDR e integración local aprobadas con Rust 1.99.0 en Linux x86_64. La integración comprobó ambos órdenes de inicio, reasociación, entrega secuencial, NMEA válido/inválido y cierre con Ctrl+C. El mínimo declarado de Rust 1.88.0 se verificó mediante los requisitos de las dependencias; no se ejecutó con ese toolchain.

## 4. Ejecutar la fuente sintética

Primero, **MV B suscriptora**, desde la raíz:

```bash
cd "$HOME/projects/nmea-dds-demo"
bash run.sh rustdds subscriber
```

Resultado esperado: `DDS Domain 0, participant id 0, tipo Marine::Navigation` y `Suscriptor listo. Esperando MarineNavigation. Ctrl+C para terminar.`

Después, **MV A publicadora**:

```bash
cd "$HOME/projects/nmea-dds-demo"
bash run.sh rustdds publisher --source synthetic
```

Debe indicar `Publicador listo. Fuente: synthetic. Ctrl+C para terminar.` y producir aproximadamente una muestra por segundo:

```text
PUBLICADO DDS: seq=12 speed=5.2 lat=10.00012 lon=-75.00012 course=12.0
```

Al asociarse, el publicador imprime un evento `DDS writer: PublicationMatched` con los detalles de la biblioteca. El suscriptor recibe:

```text
RECIBIDO DDS:
PARSEADO: seq=12 lat=10.00012 lon=-75.00012 speed=5.2 kn course=12.0 heading=14.0 depth=8.4 m position_valid=true simulated=true
```

El texto después de `RECIBIDO DDS:` está vacío porque la fuente sintética no contiene NMEA original. `PUBLICADO DDS` confirma aceptación local; `RECIBIDO DDS` y `PARSEADO` en la otra MV confirman entrega. Esperar hasta 30 segundos antes de diagnosticar la asociación. La primera secuencia puede no ser uno; los saltos posteriores se indican mediante `SALTO secuencia`.

Para una comprobación nueva, observar un minuto de recepción; esta duración es una recomendación, no una duración confirmada de la prueba reportada. Detener ambos procesos con Ctrl+C. **Invertir los roles** ejecutando el comando subscriber en MV A y publisher en MV B. El usuario reportó recepción sintética exitosa en ambos sentidos con RustDDS **0.11.2**. Ese resultado no valida **0.14.3**: la nueva prueba entre MV está **pendiente**. Usar **0.14.3 en ambas MV**, observar un minuto de recepción en cada sentido y guardar versiones, comandos y logs. Confirmar recepción continua después de la asociación, sin errores de publicación ni saltos posteriores de secuencia.

## 5. Opciones y capacidades

| Opción | Predeterminado | Efecto |
| --- | --- | --- |
| `--help`, `-h` | — | Mostrar ayuda y salir |
| `--expected-id N` | `0` | Comprobar el ID asignado por RustDDS; no asigna ID ni interfaz |
| `--source synthetic\|nmea` | `synthetic` | Elegir la fuente del publicador |
| `--nmea-listen IP:PUERTO` | `127.0.0.1:3100` | Dirección de escucha UDP del publicador NMEA |

El parser es compartido, pero las opciones de fuente no cambian el comportamiento del suscriptor. No existen opciones `--duration`, `--local` ni `--peer`. El lanzador usa `cargo run --locked` y compila si hace falta. También se pueden ejecutar `./target/debug/publisher --source synthetic` y `./target/debug/subscriber` desde la raíz.

Para diagnóstico:

```bash
RUST_LOG=info bash run.sh rustdds publisher --source synthetic
bash run.sh rustdds publisher --help
```

NMEA es una capacidad disponible, **no validada entre las MV en la prueba reportada**. El publicador admite `--source nmea --nmea-listen IP:PUERTO`, valida checksum GPRMC, conserva `raw_nmea` y publica posición/velocidad/rumbo. El endpoint predeterminado solo recibe envíos originados en esa MV. GPRMC no aporta heading ni profundidad: se muestran como `N/D`; `simulated=false`. Las líneas rechazadas no se publican. Esta guía no incluye un recorrido de simulador.

## 6. Diagnóstico

Sin recepción, revisar [diagnóstico por síntoma](../MULTICAST.md). Si el ID es inesperado, cerrar otros demos de dominio 0; ambos DDS deben ejecutarse por separado. Revisar interfaces y locators mediante el observador común a ambos DDS: el demo no limita una interfaz. Usar `ss -lunp` para puertos y guardar ambas consolas. Una asociación sin datos requiere revisar errores de publicación y tráfico de retorno, no solo SPDP.

Para la función y el momento de ejecución de todos los scripts, consultar la [tabla de scripts Python](../README.md#scripts-python-y-momento-de-ejecución). `tools/udp_probe.py` y `tools/capture_spdp.py` se ejecutan manualmente para diagnóstico según [MULTICAST.md](../MULTICAST.md); `tools/tests/test_capture_spdp.py` prueba el observador con paquetes simulados, fuera del flujo de muestras.
