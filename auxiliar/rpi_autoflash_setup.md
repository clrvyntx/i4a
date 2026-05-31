# Raspberry Pi como estacion de auto-flash ESP

## Objetivo

Estos archivos convierten la Raspberry Pi en una herramienta de flasheo:

- detecta cuando aparece un puerto serie USB
- valida si el dispositivo responde como Espressif
- si valida, flashea automaticamente
- usa un LED de la Raspberry como estado visual

Archivos:

- [rpi_esp_autoflash_daemon.py](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/rpi_esp_autoflash_daemon.py>)
- [i4a-rpi-autoflash.service](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/i4a-rpi-autoflash.service>)
- [install_rpi_autoflash.sh](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/install_rpi_autoflash.sh>)

## LED disponible

En Raspberry Pi comun normalmente hay un LED verde de actividad `ACT`. La documentacion oficial indica que el LED verde representa actividad de almacenamiento en la mayoria de los modelos, y en Zero/Zero W/Zero 2 W ese LED tambien representa el estado del sistema.

Para verificar en tu unidad:

```bash
ls /sys/class/leds
```

Si aparece alguno de estos nombres, el daemon lo puede usar:

- `ACT`
- `led0`
- `act`

Si no aparece ninguno, el daemon puede correr igual pero sin LED visual, o despues le agregamos un LED externo por GPIO.

## Semantica del LED

El daemon implementa:

- apagado: no hay ESP conectado
- titilando: flasheo en progreso
- encendido fijo: flasheo exitoso y el ESP sigue conectado

Si enchufas algo que no responde como ESP, lo ignora y deja el LED apagado.

## Como valida que sea ESP

Antes de flashear, el daemon ejecuta:

```bash
python -m esptool --port /dev/ttyUSB0 read-mac
```

Si `esptool` logra leer la MAC, el puerto se considera Espressif valido. Si falla, no flashea.

## Flujo recomendado de archivos en la Raspberry

Suponiendo usuario `i4a`:

```text
/home/i4a/i4a-flasher/
  .venv/
  esp_rpi_flasher.py
  rpi_esp_autoflash_daemon.py
  install_rpi_autoflash.sh
  firmware_bundle_idf/
    flash_args
    bootloader/bootloader.bin
    partition_table/partition-table.bin
    main.bin
```

## Preparacion inicial en la Raspberry

### 1. Paquetes del sistema

```bash
sudo apt update
sudo apt install -y python3-venv python3-pip
```

### 2. Virtualenv

```bash
mkdir -p ~/i4a-flasher
cd ~/i4a-flasher
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install esptool pyserial
```

### 3. Copiar archivos

Copia desde Windows a la Raspberry:

- `auxiliar/esp_rpi_flasher.py`
- `auxiliar/rpi_esp_autoflash_daemon.py`
- `auxiliar/install_rpi_autoflash.sh`
- el bundle de firmware

## Prueba manual antes del arranque automatico

Parado en `~/i4a-flasher`:

```bash
source .venv/bin/activate
python rpi_esp_autoflash_daemon.py --flash-args /home/i4a/i4a-flasher/firmware_bundle_idf/flash_args
```

Ahora:

1. sin ESP conectado: LED apagado
2. conectas un ESP por USB
3. si valida: LED titila y luego queda encendido
4. cuando desconectas el ESP: LED apagado otra vez

## Instalacion para que arranque siempre al boot

### Opcion recomendada: instalador

Desde `~/i4a-flasher`:

```bash
chmod +x install_rpi_autoflash.sh
sudo ./install_rpi_autoflash.sh --flash-args /home/i4a/i4a-flasher/firmware_bundle_idf/flash_args
```

Si usas bundle simple:

```bash
sudo ./install_rpi_autoflash.sh \
  --bin-dir /home/i4a/i4a-flasher/firmware_bundle_simple \
  --app firmware.bin
```

### Lo que hace el instalador

- genera `/etc/systemd/system/i4a-rpi-autoflash.service`
- recarga systemd
- habilita el servicio
- lo reinicia

## Comandos de systemd

### Ver estado

```bash
sudo systemctl status i4a-rpi-autoflash.service
```

### Ver logs en vivo

```bash
sudo journalctl -u i4a-rpi-autoflash.service -f
```

### Reiniciar

```bash
sudo systemctl restart i4a-rpi-autoflash.service
```

### Deshabilitar

```bash
sudo systemctl disable --now i4a-rpi-autoflash.service
```

## Consideraciones practicas

- El daemon toma control del LED `ACT` y por eso deja de mostrar actividad de almacenamiento mientras el servicio esta corriendo.
- Si queres conservar el LED interno para actividad del sistema, conviene usar un LED externo en un GPIO y adaptar el script.
- El flasheo automatico asume que lo que enchufas es un adaptador USB-serie accesible como `/dev/ttyUSB*` o `/dev/ttyACM*`.
- Si el ESP no entra automaticamente en bootloader por DTR/RTS, el daemon no va a poder flashearlo sin intervencion fisica.

## Comando para verificar el LED antes de instalar

```bash
ls /sys/class/leds
cat /sys/class/leds/ACT/trigger
```

Si `ACT` no existe, proba:

```bash
cat /sys/class/leds/led0/trigger
```
