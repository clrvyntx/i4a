# Flujo con contenedor

El repo ahora incluye un wrapper principal:

- [i4a](</c:/Users/mroch/Documents/Fiuba/i4a_project/i4a>)

La idea es que no tengas que invocar `docker run ...` a mano. El script construye una imagen local basada en `espressif/idf:v5.1.2`, monta este repo dentro del contenedor y ejecuta todo desde `app/`.

## Requisitos

- Docker instalado
- Acceso al daemon Docker
- En Linux/Raspberry, permisos para usar el puerto serie (`dialout` o equivalente)

## Primer uso

```bash
chmod +x ./i4a
./i4a image build
```

Despues ya podes usar los comandos directos.

## Comandos principales

```bash
./i4a build
./i4a clean
./i4a fullclean
./i4a menuconfig
./i4a shell
./i4a list-ports
./i4a flash /dev/ttyUSB0
./i4a flash-many /dev/ttyUSB0 /dev/ttyUSB1
./i4a erase-flash /dev/ttyUSB0
./i4a monitor /dev/ttyUSB0
./i4a idf build
./i4a idf -p /dev/ttyUSB0 flash
./i4a run python --version
```

## Que hace `./i4a`

- construye la imagen si no existe;
- monta el repo completo en `/workspace`;
- entra a `/workspace/app`;
- ejecuta `idf.py` o el comando que le pidas;
- para flash y monitor monta el puerto serie como dispositivo Docker.

## Notas sobre puertos serie

Para listar puertos:

```bash
./i4a list-ports
```

Si no tenes permisos en Raspberry/Linux:

```bash
sudo usermod -a -G dialout $USER
```

Despues cerra sesion o reinicia.

## Variables utiles

```bash
I4A_IMAGE_NAME=i4a-esp-idf:v5.1.2
I4A_DOCKER_BIN=docker
I4A_FLASH_BAUD=921600
I4A_MONITOR_BAUD=115200
I4A_HOME_CACHE=.i4a-home
I4A_DOCKER_RUN_ARGS="--network host"
I4A_FORCE_REBUILD=1
```

Ejemplo:

```bash
I4A_FORCE_REBUILD=1 ./i4a build
```

## Imagen y entrypoint

Archivos involucrados:

- [Dockerfile](</c:/Users/mroch/Documents/Fiuba/i4a_project/Dockerfile>)
- [docker/i4a-container.sh](</c:/Users/mroch/Documents/Fiuba/i4a_project/docker/i4a-container.sh>)

El `Dockerfile` usa ESP-IDF `v5.1.2`, que coincide con lo documentado en el proyecto.
