# Flasheador ESP32 con Raspberry Pi

## Objetivo

Esta carpeta agrega un flasheador pensado para Raspberry Pi sin entorno grafico y sin necesidad de instalar todo ESP-IDF en la Raspberry.

Archivo principal:

- [esp_rpi_flasher.py](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/esp_rpi_flasher.py>)

El script usa:

- `python3`
- `esptool`
- `pyserial` para el monitor serie

## Idea de trabajo

Hay dos formas de usarlo:

1. **Modo recomendado**: copiar a la Raspberry el archivo `flash_args` generado por ESP-IDF junto con los `.bin`.
2. **Modo simple**: copiar `bootloader.bin`, `partition-table.bin` y el binario de la app, y usar offsets estandar.

El modo `flash_args` es mejor porque evita hardcodear offsets u opciones de flash si cambian en el futuro.

## Preparacion en la Raspberry

```bash
sudo apt update
sudo apt install -y python3-venv python3-pip
mkdir -p ~/i4a-flasher
cd ~/i4a-flasher
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install esptool pyserial
```

Despues, mientras el `venv` este activo, usa `python ...` en lugar de `python3 ...`.

## Detectar puertos serie

```bash
python esp_rpi_flasher.py list
```

Tambien se puede usar:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

## Como generar el bundle desde una maquina con ESP-IDF

Desde este repo:

```bash
cd app
idf.py build
```

Despues, conviene copiar desde `app/build/` al bundle que ira a la Raspberry:

- `flash_args`
- `bootloader/bootloader.bin`
- `partition_table/partition-table.bin`
- el binario final de la app, por ejemplo `main.bin` o el nombre real del proyecto

Una estructura practica queda asi:

```text
firmware_bundle/
  flash_args
  bootloader/bootloader.bin
  partition_table/partition-table.bin
  main.bin
```

Si no queres usar `flash_args`, tambien sirve un bundle plano:

```text
firmware_bundle/
  bootloader.bin
  partition-table.bin
  firmware.bin
```

En ese caso, `firmware.bin` puede ser un rename del binario final de la app.

## Flasheo usando `flash_args`

Ejemplo para un solo puerto:

```bash
python esp_rpi_flasher.py flash \
  --port /dev/ttyUSB0 \
  --flash-args /home/pi/firmware_bundle/flash_args
```

Ejemplo para varios puertos:

```bash
python esp_rpi_flasher.py flash \
  --port /dev/ttyUSB0 \
  --port /dev/ttyUSB1 \
  --port /dev/ttyUSB2 \
  --flash-args /home/pi/firmware_bundle/flash_args
```

Ejemplo flasheando todos los puertos detectados:

```bash
python esp_rpi_flasher.py flash \
  --all-detected \
  --flash-args /home/pi/firmware_bundle/flash_args
```

## Flasheo usando bundle simple

Con layout estandar:

- bootloader en `0x1000`
- partition table en `0x8000`
- app en `0x10000`

Ejemplo:

```bash
python esp_rpi_flasher.py flash \
  --port /dev/ttyUSB0 \
  --bin-dir /home/pi/firmware_bundle
```

Si el binario de app no se llama `firmware.bin`, se puede indicar:

```bash
python esp_rpi_flasher.py flash \
  --port /dev/ttyUSB0 \
  --bin-dir /home/pi/firmware_bundle \
  --app main.bin
```

## Flashear solo la app

Esto sirve si el ESP ya tiene un bootloader y una partition table compatibles.

```bash
python esp_rpi_flasher.py flash \
  --port /dev/ttyUSB0 \
  --bin-dir /home/pi/firmware_bundle \
  --app main.bin \
  --app-only
```

## Borrado completo antes de flashear

```bash
python esp_rpi_flasher.py flash \
  --port /dev/ttyUSB0 \
  --bin-dir /home/pi/firmware_bundle \
  --erase-first
```

## Monitor serie

```bash
python esp_rpi_flasher.py monitor --port /dev/ttyUSB0
```

Con baudrate explicito:

```bash
python esp_rpi_flasher.py monitor \
  --port /dev/ttyUSB0 \
  --baud 115200
```

## Permisos en Raspberry

Si aparece `Permission denied` al abrir `/dev/ttyUSB0`, normalmente alcanza con agregar el usuario al grupo `dialout`:

```bash
sudo usermod -a -G dialout $USER
```

Despues hay que cerrar sesion o reiniciar la Raspberry.

## Resumen del comportamiento del script

- `list`: lista `/dev/ttyUSB*` y `/dev/ttyACM*`
- `flash`: flashea uno o varios puertos
- `monitor`: abre consola serie con `pyserial`

Detalles utiles:

- `--port` es repetible
- `--all-detected` permite flasheo masivo secuencial
- `--flash-args` reutiliza la salida de ESP-IDF
- `--app-only` permite actualizar solo firmware de aplicacion

## Recomendacion para este repo

Para `i4a_project`, la opcion mas segura es:

1. Compilar en una maquina con ESP-IDF.
2. Copiar a la Raspberry el `flash_args` y los `.bin` generados en `app/build/`.
3. Ejecutar el flasheo desde Raspberry con `--flash-args`.

Asi se evita depender del entorno de build en la Raspberry y tambien se evita fijar nombres u offsets a mano.
