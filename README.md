# NMEA DDS demo — etapa 1: RustDDS y datos secuenciales

## Alcance y estado

Un solo proyecto, un solo IDL. Publicador RustDDS de Atostek en WSL; suscriptor
RustDDS en Ubuntu VMware. Datos sintéticos a 1 Hz, marcados simulated=true.
La integración NMEA y la implementación OpenDDS están pendientes.
La carpeta opendds/ reserva el lugar: no contiene todavía una implementación.

Validado al preparar esta entrega: ambos binarios compilan con Rust 1.99.0;
la prueba de serialización/deserialización CDR de todos los campos pasa.
El entorno de preparación no permite enumerar interfaces ni recibir multicast;
por tanto, la comunicación entre dos participantes y el auxiliar RTPS NO se
han validado en ejecución aquí. Deben comprobarse en las máquinas del usuario.

Se fija RustDDS 0.11.2, cuya API se inspeccionó; no es una afirmación de que sea
la versión más reciente. Cargo.lock fija las dependencias. Ejecutar --locked.

## Arquitectura

WSL 192.168.10.15: generador secuencial -> DataWriter RustDDS.
VM 192.168.10.33: DataReader RustDDS -> consola.

Domain 0; Topic MarineNavigation; tipo Marine::Navigation; NoKey;
Reliability Reliable; History KeepLast(10). Una instancia de participante por
OS, con participant id 0. No ejecutar simultáneamente otros demos en domain 0.

DDS sigue intercambiando información de participantes y extremos. No hay
asociación completamente estática: los auxiliares hacen posible ese intercambio
usando direcciones conocidas. No se requiere multicast ENTRE máquinas.

Cada ejecutable inicia automáticamente tools/rtps_unicast_bridge.py en su OS.
El auxiliar escucha la emisión multicast LOCAL de RustDDS en 239.255.0.1,
puertos 7400 (metadatos) y 7401 (datos), y reenvía paquetes RTPS sin modificar
hacia la IP fija del otro OS, puertos unicast 7410 y 7411 (domain 0, id 0).
Solo reenvía datagramas cuyo origen coincide con la IP local elegida; los paquetes
remotos entran por puertos distintos y no se reenvían, evitando bucles.
Los ACK y el tráfico que RustDDS envía directamente por unicast van directamente
entre participantes. El auxiliar no es un discovery server ni una función nativa
initial_peers de RustDDS. Necesita multicast LOCAL funcional y UDP unicast en
ambos sentidos. No incorpora DDS Security, retransmisión ni reescritura de NAT.
Es una solución auxiliar para este demo, no una recomendación de despliegue final.

## Archivos

- idl/Navigation.idl: fuente única de tipos.
- tools/idl_to_rust.py: generador limitado, invocado por build.rs.
- rustdds/src/lib.rs: configuración, auxiliar y generador de muestras.
- rustdds/src/bin/publisher.rs: DataWriter.
- rustdds/src/bin/subscriber.rs: DataReader y comprobación de secuencia.
- tools/udp_probe.py: diagnóstico unicast con ACK, puerto 17400.
- run.sh: selector de implementación y rol.
- opendds/: etapa posterior; reutilizará el mismo IDL.

El generador admite un module, un @topic struct sin clave y los tipos unsigned
long, unsigned long long, long, double y boolean. Rechaza sintaxis distinta.
No es un compilador IDL general. Los tipos Rust se generan dentro de target/;
no se mantienen manualmente en paralelo con el IDL.

## 1. Preparación en WSL y MV

Mantener WSL mirrored y VMware Bridged. Confirmar las IP actuales con:

```bash
ip -br -4 addr
```

En AMBOS Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential pkg-config python3 git curl ca-certificates unzip
rustc --version
cargo --version
```

Solo si Rust/Cargo no existen, instalar en ese Ubuntu:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
```

Seleccionar la instalación estándar (opción 1).

Descargar el ZIP de esta entrega en ~/Downloads en cada Ubuntu. En WSL también
puedes usar el ZIP descargado por Windows bajo /mnt/c/Users/TU_USUARIO/Downloads.
Copiar el ZIP a la MV es suficiente; no copiar target/ ni binarios de Windows.
En cada Ubuntu:

```bash
mkdir -p ~/projects
unzip ~/Downloads/nmea-dds-demo.zip -d ~/projects
cd ~/projects/nmea-dds-demo
cargo build --locked --workspace --bins
cargo test --locked --workspace
```

Si usas el ZIP de Windows en WSL, reemplaza SOLO la ruta del ZIP en unzip.
No necesitas VS Code instalado en la MV para compilar y ejecutar.

## 2. Prueba de retorno unicast

Cerrar los scripts anteriores de prueba, para liberar UDP 17400.
En la MV, desde la raíz del proyecto:

```bash
python3 tools/udp_probe.py listen --local 192.168.10.33
```

Mientras espera, en WSL:

```bash
python3 tools/udp_probe.py send --local 192.168.10.15 --peer 192.168.10.33
```

WSL debe imprimir OK: UDP unicast funciona en ambos sentidos.
La MV responde y termina. El probe no usa DDS. Las reglas Windows de la prueba
anterior ya cubren 17400 desde 192.168.10.33. Si aparece TIMEOUT, resolver el
retorno unicast antes de esperar comunicación DDS.

## 3. Firewall DDS en Windows

PowerShell COMO ADMINISTRADOR, fuera de WSL. Ejecutar una sola vez:

```powershell
New-NetFirewallHyperVRule `
    -Name "NMEA-Demo-DDS-Unicast" `
    -DisplayName "NMEA DDS - UDP desde VMware hacia WSL" `
    -Direction Inbound `
    -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
    -Protocol UDP `
    -LocalPorts "7410-7411" `
    -RemoteAddresses "192.168.10.33" `
    -Action Allow

New-NetFirewallRule `
    -Name "NMEA-Demo-DDS-Unicast-Windows" `
    -DisplayName "NMEA DDS - UDP desde VMware" `
    -Direction Inbound `
    -Protocol UDP `
    -LocalPort "7410-7411" `
    -RemoteAddress "192.168.10.33" `
    -Profile Any `
    -Action Allow
```

Si la IP de la MV cambia, actualizar también el origen de estas reglas.
No desactivar el firewall. Las reglas anteriores de puerto 17400 no cubren DDS.
Si la MV tiene UFW instalado y activo, ejecutar EN LA MV:

```bash
sudo ufw allow from 192.168.10.15 to any port 7410:7411 proto udp
```

No es necesario instalar UFW si no está instalado.

## 4. Ejecutar

Primero EN LA MV, desde ~/projects/nmea-dds-demo:

```bash
bash run.sh rustdds subscriber --local 192.168.10.33 --peer 192.168.10.15
```

Después EN WSL, desde la misma ruta del proyecto:

```bash
bash run.sh rustdds publisher --local 192.168.10.15 --peer 192.168.10.33
```

El auxiliar arranca dentro del ejecutable, no requiere otra terminal.
Puede haber varios segundos de espera (los anuncios de participante son periódicos).
Esperar hasta 30 segundos antes de diagnosticar. PUBLICADO solo confirma que
RustDDS aceptó la muestra; la evidencia de entrega es RECIBIDO en la MV.
PublicationMatched en el publicador confirma asociación con un lector.

Ejemplo de recepción:

```
RECIBIDO seq=12 lat=10.00012 lon=-75.00012 speed=5.2 kn course=12.0 heading=14.0 depth=8.4 m simulated=true
```

La primera secuencia recibida puede no ser 1 porque la asociación tarda y la
durabilidad es volátil. Los saltos posteriores se informan en consola.
Estas muestras no representan un recorrido físico coherente: prueban el transporte.
Los timestamps no deben usarse como medida de latencia sin sincronizar relojes.

Mantener la prueba un minuto. Ctrl+C termina el ejecutable y su auxiliar.
Para repetir, iniciar de nuevo el suscriptor y el publicador.

## 5. Si no recibe

- Error en auxiliar: comprobar IP local, python3 y multicast local. No usar sudo
  para ejecutar cargo; corregir la configuración si los sockets están restringidos.
- Participant id inesperado: cerrar otros demos domain 0. Se espera id 0 en cada OS.
- En ambos lados debe aparecer [RTPS bridge] con destino del par y puerto 7410.
  La línea 7411 solo aparece si hay emisión local multicast de datos: el tráfico
  directo unicast no pasa por el auxiliar.
- Captura EN WSL mientras ejecutas el demo:

```bash
sudo tcpdump -ni any 'udp and src host 192.168.10.33 and (dst port 7410 or dst port 7411)'
```

- Captura EN LA MV:

```bash
sudo tcpdump -ni any 'udp and src host 192.168.10.15 and (dst port 7410 or dst port 7411)'
```

- Diagnóstico Rust:

```bash
RUST_LOG=info bash run.sh rustdds publisher --local 192.168.10.15 --peer 192.168.10.33
```

Guardar las salidas de ambas consolas si falla. No confundir el éxito de compilación
con el éxito de comunicación. El auxiliar no puede resolver un bloqueo unicast.

## 6. Un único repositorio GitHub

Se puede empezar sin GitHub. Cuando la prueba funcione, crear en GitHub un
repositorio vacío nmea-dds-demo (sin README inicial). En WSL, raíz del proyecto:

```bash
git init
git add .
git commit -m "Add sequential RustDDS demo with shared IDL"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/nmea-dds-demo.git
git push -u origin main
```

Reemplazar TU_USUARIO. Autenticarse por el mecanismo habitual de GitHub; no usar
la contraseña de la cuenta como contraseña HTTPS. Si Git pide identidad, configurar
user.name y user.email con los datos del usuario. En la MV se podrá clonar ese mismo
repositorio y usar git pull para las siguientes etapas. No crear otro repositorio
para OpenDDS.

## 7. Siguiente etapa

1. Añadir OpenDDS publisher/subscriber al mismo repo, generados desde
   idl/Navigation.idl. Primero transmitir las mismas muestras sintéticas.
2. Ampliar run.sh para seleccionar opendds además de rustdds.
3. Conectar la fuente NMEA Windows -> WSL por UDP 127.0.0.1:3100.
4. Sustituir sequential() por una fuente que valide checksum, estado GPS,
   grados/minutos y unidades. Diferenciar course over ground de heading true.
   Añadir indicadores de validez/antigüedad para datos opcionales como profundidad
   antes de publicar entradas reales. simulated=false solo para esa fuente.
5. Mantener el contrato IDL y los tópicos iguales entre máquinas de cada prueba.
   No se requiere comunicación cruzada OpenDDS/RustDDS para este proyecto.

## 8. Fuente GPS GPRMC por UDP

El publicador RustDDS puede recibir sentencias GPRMC desde un simulador que envía
datagramas UDP. El flujo es:

```text
simulador Windows -> UDP 127.0.0.1:3100 en WSL -> publicador RustDDS -> DDS -> suscriptor
```

Iniciar el suscriptor en la MV como siempre y, en WSL, ejecutar:

```bash
bash run.sh rustdds publisher \
  --local 192.168.10.15 \
  --peer 192.168.10.33 \
  --source nmea \
  --nmea-listen 127.0.0.1:3100
```

El publicador acepta líneas que contengan una sentencia `$GPRMC`, incluso si
incluyen un prefijo como `GPS1 on UDP2:`. Valida el checksum NMEA, convierte
latitud y longitud a grados decimales, conserva velocidad en nudos y publica
el rumbo sobre el suelo en `course_deg`. Las sentencias rechazadas se informan
en la consola y no se publican.

La muestra DDS conserva también la sentencia completa en `raw_nmea`. El
publicador imprime la línea recibida del simulador antes de publicar y el
suscriptor imprime esa misma línea al recibirla, además de un resumen de los
campos parseados. Esto permite comparar directamente ambos extremos.

GPRMC no contiene rumbo verdadero ni profundidad. Por ello `heading_valid` y
`depth_valid` se publican como `false`, con valores numéricos `0.0`; el
suscriptor los muestra como `N/D`. `position_valid` refleja el estado `A` o
`V` de GPRMC y `simulated=false` identifica datos recibidos desde esta fuente.
La secuencia DDS solo avanza para muestras GPRMC válidas.

El publicador queda escuchando datagramas UDP; no requiere una conexión persistente
ni una fase de reconexión. Cada datagrama puede contener una o varias líneas.
El modo sintético original sigue disponible para diagnóstico:

```bash
bash run.sh rustdds publisher \
  --local 192.168.10.15 \
  --peer 192.168.10.33 \
  --source synthetic
```

Fuentes: RustDDS 0.11.2, código del paquete publicado y ejemplos oficiales;
https://github.com/Atostek/RustDDS ; https://docs.rs/rustdds/0.11.2/ .
Configuración WSL/firewall:
https://learn.microsoft.com/en-us/windows/wsl/networking .
