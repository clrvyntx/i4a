# Contexto del repo `i4a_project`

## Alcance de este documento

Este archivo resume el estado actual del repositorio, su arquitectura y los flujos principales para que una persona nueva pueda ubicarse rapido sin releer todo el codigo. Fue escrito a partir de una recorrida manual del repo y no implica cambios funcionales.

## Resumen ejecutivo

- El repo implementa una arquitectura de red distribuida para ESP32 orientada a mesh liviano.
- La base principal es firmware sobre ESP-IDF, con utilidades auxiliares en Python/Bash para el gateway.
- La topologia combina:
  - comunicacion interna entre placas del mismo nodo por SPI en anillo;
  - comunicacion externa entre nodos por WiFi;
  - ruteo simplificado basado en roles, provisionamiento y tablas chicas.
- El proyecto no muestra tests automatizados dentro del repo.
- El punto de build no es la raiz del repo sino `app/`, que importa componentes desde `../components`.

## Estructura general

### `app/`

Proyecto ESP-IDF principal.

- `app/CMakeLists.txt`
  - Define `EXTRA_COMPONENT_DIRS` apuntando a `../components/wireless`, `../components/internal`, `../components/routing` y `../components/integration`.
  - Usa `../components/sdkconfig.defaults`.
- `app/main/main.c`
  - Punto de entrada del firmware.
  - Inicializa `node`, `sync`, `shared_state` y `routing`.
  - Elige el rol del dispositivo y arranca el hook de ruteo.

### `components/`

Se divide en cuatro familias:

- `components/wireless`
  - Abstracciones de WiFi (`device`, `access_point`, `station`, `client`, `server`, wrappers de `wireless`).
- `components/internal`
  - Transporte local SPI (`ring_link`, `ring_link_lowlevel`, `ring_link_internal`, `ring_link_netif`, `spi`, `config`, hooks lwIP).
- `components/routing`
  - Logica de ruteo, sincronizacion distribuida y estado compartido.
- `components/integration`
  - Pegamento entre capas: `node`, `callbacks`, `internal_messages`, `channel_manager`, `reset_manager`, `info_manager`, `remote_control`, `traffic`, `routing_hooks`.

### `gateway/`

Scripts que corren fuera del ESP32:

- `info_server.py`
  - Flask que recibe JSON por HTTP.
- `remote_control.py`
  - Cliente TCP para enviar comandos remotos a un nodo.
- `99-run-network-setup.sh`
  - Script de NetworkManager para habilitar forwarding/NAT y rutas.

### `docs/`

Imagenes de arquitectura, SPI, FreeRTOS, lwIP y distribucion de red. Sirven como contexto de alto nivel, no como codigo ejecutable.

## Modelo de despliegue

El sistema modela un **nodo** como un conjunto de entre 2 y 5 placas ESP32 conectadas en anillo por SPI. Cada placa cumple una orientacion fisica:

- North
- South
- East
- West
- Center

El nodo completo expone conectividad externa por WiFi. La placa central cumple un papel especial y puede actuar como root o como nodo "home" interno segun la configuracion de hardware.

## Configuracion por hardware

La identidad del dispositivo se lee por GPIO en `components/internal/config`:

- `CONFIG_PIN_0 -> GPIO 22`
- `CONFIG_PIN_1 -> GPIO 21`
- `CONFIG_PIN_2 -> GPIO 16`

Los 3 bits determinan:

- orientacion `N/S/E/W` para los peer-link devices (`0**`);
- modo `ACCESS_POINT` o `ROOT` cuando el bit alto esta en `1`.

Ademas, `config.c` asigna IPs internas para las netifs SPI:

- RX: `192.168.0.(orientation + 1)`
- TX: `192.168.0.(orientation + 10)`

## Flujo de arranque

### 1. `node_setup()`

`components/integration/node/node.c` hace el bootstrap grueso:

- crea colas y tasks de callbacks;
- lee configuracion por pines;
- inicializa mensajes internos (`ring_share`, `channel_manager`, `info_manager`, `reset_manager`);
- espera una ventana escalonada para evitar picos de corriente;
- inicializa WiFi y `ring_link`;
- coordina reset/startup entre placas del mismo nodo;
- guarda MAC/UUID/rol root;
- arranca scheduler de telemetria interna.

### 2. `app_main()`

`app/main/main.c`:

- toma instancias compartidas de `wireless`, `ring_share` y `routing`;
- inicializa:
  - `sync` (token ring para secciones criticas),
  - `shared_state`,
  - `routing`;
- decide el rol:
  - center root -> `rt_init_root()`;
  - center no root -> `rt_init_home()`;
  - resto -> `rt_init_forwarder()`;
- registra el hook de ruteo adecuado;
- arranca `rt_on_start()` y la tarea periodica `routing_task`;
- si corresponde, pone el dispositivo como `AP` o `STA`.

## Transporte interno: SPI + Ring Link

La capa local vive en `components/internal`.

### `ring_link`

`ring_link.c` recibe payloads desde la capa low-level y los clasifica en:

- payloads internos;
- payloads de `esp_netif`.

Cada categoria va a una queue y una task distinta.

### `spi`

`spi.c` implementa el low-level con:

- pool fijo de buffers DMA;
- cola de buffers libres;
- cola de recepcion;
- tarea de polling para el lado slave;
- `spi_device_transmit()` para el lado master.

### `ring_link_internal`

Procesa payloads internos:

- si son broadcast, delega al manejador correspondiente;
- si son para la placa actual, los convierte en callbacks de siblings;
- si no son para esta placa, los reenvia.

### `ring_link_netif`

Expone el anillo SPI como interfaz compatible con `esp_netif`/lwIP:

- TX encapsula paquetes IP dentro de `ring_link_payload_t`;
- RX reconstruye `pbuf`s y entrega paquetes a `esp_netif_receive()`.

En la practica, el SPI local aparece como un camino de red mas para el algoritmo de ruteo.

## Bus interno entre modulos: `ring_share`

`components/routing/ring_share` multiplexa mensajes broadcast del nodo segun `component_id`.

Componentes registrados hoy:

- `RS_SYNC`
- `RS_ROUTING`
- `RS_SHARED_STATE`
- `RS_CHANNEL_MANAGER`
- `RS_RESET_MANAGER`
- `RS_INFO_MANAGER`

La idea es simple: el primer byte del mensaje identifica al consumidor, y luego cada modulo procesa su payload.

## Sincronizacion distribuida: `sync`

`components/routing/sync` implementa un **token ring** por componente. Esto resuelve una necesidad central del diseno: varias placas del mismo nodo comparten estado logico, pero no deben modificarlo en paralelo.

Puntos importantes:

- cada `RS_*` puede tener su propia seccion critica;
- el `center` actua como lider del token ring;
- las callbacks registradas se ejecutan cuando la placa obtiene el token;
- `routing` usa este mecanismo para despachar eventos y refrescar estado compartido.

## Estado compartido: `shared_state`

`components/routing/shared_state` replica blobs opacos entre placas del mismo nodo.

Uso actual relevante:

- `routing` registra su tabla de ruteo con `ss_watch()`;
- cuando entra en seccion critica, puede propagar la copia actual al resto del nodo mediante `ss_refresh()`.

Esto evita que cada placa mantenga topologia global independiente por fuera del protocolo interno.

## Wireless: modos, enlaces y mensajeria entre nodos

La capa `components/wireless` abstrae AP, STA y AP+STA.

### `device`

`device.c` centraliza:

- inicializacion WiFi global (`esp_netif`, event loop, `esp_wifi_init`);
- armado de SSID y password;
- cambio de modo (`AP`, `STATION`, `AP_STATION`);
- seleccion de la netif activa;
- envio de mensajes wireless;
- deteccion de si un destino pertenece al enlace point-to-point actual.

### `server` y `client`

La comunicacion entre placas de nodos distintos usa TCP:

- `server.c`
  - escucha en puerto `3999`;
  - se activa sobre APs no centrales cuando se conecta una STA;
  - genera eventos `peer_connected`, `peer_message`, `peer_lost`.
- `client.c`
  - conecta al gateway de la interfaz STA, tambien por `3999`;
  - reintenta en loop hasta lograr conexion;
  - genera los mismos eventos hacia la capa superior.

### Comportamiento AP/STA

Segun `node_set_as_ap()` y `node_set_as_sta()`:

- center root:
  - AP para la red NAT final;
  - SSID `Internet4All_Root`;
  - password `I4A123456`;
  - mascara `/30` en el borde WAN/LAN.
- center no root:
  - AP de casa;
  - SSID `ComNetAR`;
  - password vacia;
  - arranca servidor de control remoto.
- placas no centrales:
  - AP o AP+STA para enlaces entre nodos;
  - SSID con prefijo `I4A`;
  - password `zWfAc2wXq5`;
  - subredes `/30`.

## Routing: como decide por donde sale un paquete

La logica de ruteo se reparte entre `routing/` y `integration/routing_hooks.c`.

### Hook de lwIP

`custom_ip4_route_src_hook()` en `main.c` deriva en `node_do_routing()`, que decide si un paquete va por:

- SPI;
- WiFi;
- o se resuelve con reglas especiales del rol actual.

### Roles

#### Root

`root.c`:

- fija la red raiz (`10.0.0.0/8` desde `main.c`);
- agrega el default gateway al propio nodo;
- provisiona a siblings;
- responde solicitudes de nuevo gateway tras un timeout corto.

#### Home

`home.c`:

- espera provisionamiento interno;
- calcula su subnet local a partir de la red del nodo;
- actualiza tabla de ruteo;
- habilita su AP una vez provisionado.

#### Forwarder

`forwarder.c`, `forwarder/peer.c`, `forwarder/sibling.c`:

- maneja `handshake` con peers;
- propaga `DTR` (distance-to-root);
- provisiona siblings locales;
- reacciona a perdida de gateway;
- participa en eleccion/descubrimiento de nuevo gateway.

### Tabla de ruteo

`routing_table.c` mantiene una tabla ordenada por mascara:

- insercion con prioridad a prefijos mas especificos;
- default gateway separado;
- lookup lineal;
- eliminacion por `output`.

Es una implementacion chica y acorde con el objetivo del proyecto: baja complejidad y poco estado.

## Eventos y desacople entre capas

`components/integration/callbacks.c` es una pieza importante:

- recibe eventos de siblings y peers;
- los pone en colas FreeRTOS;
- los procesa en tasks dedicadas;
- desacopla ISR/stack de red de la logica de negocio.

Esto baja el acoplamiento temporal entre transporte, routing y modulos de integracion.

## Modulos de integracion relevantes

### `reset_manager`

- coordina reset/startup entre placas del mismo nodo;
- difunde un UUID basado en la MAC;
- al root le asigna UUID fijo `000000000000`.

### `channel_manager`

- reparte canales sugeridos a siblings;
- usa dos formaciones predefinidas para evitar colisiones simples;
- puede desactivar temporalmente la STA durante la fase inicial.

### `info_manager`

- cada orientacion difunde al anillo info local cada 5 minutos;
- la placa center root postea un JSON al gateway HTTP;
- payload incluye orientacion, UUID/MAC, link, subnet, mascara, canal, RSSI y trafico acumulado.

### `remote_control`

- servidor TCP autenticado por password simple;
- hoy soporta `ap_enable` y `ap_disable`;
- se levanta en el center no root.

## Gateway externo

`gateway/` es el complemento del root:

- `info_server.py`
  - recibe POSTs JSON en `http://10.255.255.254:8000/`.
- `99-run-network-setup.sh`
  - habilita `net.ipv4.ip_forward=1`;
  - elimina una default route no deseada;
  - agrega ruta a `10.0.0.0/8` via `10.255.255.253`;
  - aplica NAT/MASQUERADE hacia `eth0`.
- `remote_control.py`
  - envia `password:command` al puerto `3999`.

## Flujo extremo a extremo resumido

1. Cada placa arranca, lee su identidad por GPIO y entra en el bootstrap comun.
2. Las placas del mismo nodo se sincronizan por SPI y difunden startup/reset.
3. `routing` se inicializa con un rol distinto segun orientacion y modo.
4. El root anuncia red; forwarders y home se provisionan en cascada.
5. Los enlaces WiFi entre nodos se forman con AP/STA y sockets TCP.
6. Los eventos wireless alimentan la tabla de ruteo y la logica de gateway.
7. Los paquetes IP salen por SPI o WiFi segun el hook de ruteo.
8. El root center publica telemetria hacia el gateway HTTP externo.

## Observaciones practicas

### Lo que esta claro

- El diseno esta pensado para hardware limitado y topologia relativamente fija.
- El repo separa bastante bien:
  - transporte local;
  - transporte wireless;
  - coordinacion interna;
  - ruteo;
  - integracion/operacion.
- Hay una intencion fuerte de minimizar tablas y evitar protocolos mesh pesados.

### Riesgos o puntos a revisar

- No aparecen tests automatizados.
- Hay varios caminos que terminan en `os_panic(...)`; el enfoque es fail-fast.
- `shared_state.c` tiene un `TODO` explicito sobre posible race condition en `ss_watch()`.
- En `routing/impl.h` aparecen `TODO` sobre limitar mejor el largo de caminos (`hag_networks[16]`).
- Hay credenciales y nombres de red hardcodeados en el codigo fuente.
- El README sugiere build desde la raiz, pero la estructura real indica que el proyecto ESP-IDF vive en `app/`.

### Suposiciones utiles para futuros cambios

- Si hay que tocar comportamiento de arranque o rol, el archivo de entrada mas importante es `components/integration/node/node.c`.
- Si hay que tocar decision de salida SPI/WiFi, mirar `components/integration/routing_hooks/routing_hooks.c` y `components/routing/routing/src`.
- Si hay que tocar transporte local, empezar por `components/internal/ring_link*` y `components/internal/spi/spi.c`.
- Si hay que tocar operacion del gateway, todo lo necesario esta concentrado en `gateway/`.

## Estado observado durante esta lectura

- Arbol git limpio al momento del analisis.
- No se detectaron archivos de test bajo patrones comunes (`test`, `spec`, `pytest`, `unittest`).

## Ruta recomendada para seguir leyendo

Si alguien necesita continuar el analisis tecnico, conviene leer en este orden:

1. `README.md`
2. `app/main/main.c`
3. `components/integration/node/node.c`
4. `components/integration/routing_hooks/routing_hooks.c`
5. `components/routing/routing/src/*.c`
6. `components/internal/ring_link*.c`
7. `components/wireless/device/device.c`
8. `components/integration/info_manager/src/info_manager.c`
9. `gateway/*`
