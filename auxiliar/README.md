# Servicio de Auto-Flash en Raspberry Pi

## Objetivo

Esta guia deja configurada una Raspberry Pi como estacion de flasheo automatica para ESP32:

- parte de un firmware ya compilado
- prepara el bundle en Windows
- copia los archivos a la Raspberry
- crea el entorno Python
- instala y habilita el servicio `systemd`
- deja a mano los comandos para revisar estado y logs

Archivos involucrados:

- [esp_rpi_flasher.py](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/esp_rpi_flasher.py>)
- [rpi_esp_autoflash_daemon.py](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/rpi_esp_autoflash_daemon.py>)
- [install_rpi_autoflash.sh](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/install_rpi_autoflash.sh>)
- [i4a-rpi-autoflash.service](</c:/Users/mroch/Documents/Fiuba/i4a_project/auxiliar/i4a-rpi-autoflash.service>)

## Suposiciones

- Ya tenes un firmware compilado por vos o por un companero.
- Vas a usar el flujo `firmware_bundle_simple`.
- El usuario de la Raspberry es `i4a`.
- La carpeta de trabajo en la Raspberry sera `/home/i4a/i4a-flasher`.
- El acceso por SSH a la Raspberry puede hacerse por hostname, IP o mediante Tailscale.

## Estructura final esperada en la Raspberry

```text
/home/i4a/i4a-flasher/
  .venv/
  esp_rpi_flasher.py
  rpi_esp_autoflash_daemon.py
  install_rpi_autoflash.sh
  i4a-rpi-autoflash.service
  firmware_bundle_simple/
    bootloader.bin
    partition-table.bin
    firmware.bin
```

## Paso 1. Preparar el bundle en Windows

Parado en la raiz del repo:

```powershell
cd C:\Users\mroch\Documents\Fiuba\i4a_project
```

### Si el binario compilado ya esta en `app/build`

Usa estas rutas:

- `.\app\build\bootloader\bootloader.bin`
- `.\app\build\partition_table\partition-table.bin`
- `.\app\build\main.bin`

### Si el binario compilado te lo paso un companero

Necesitas estos 3 archivos, aunque vengan desde otra carpeta:

- `bootloader.bin`
- `partition-table.bin`
- el binario final de la app

Si el binario final de la app no se llama `main.bin`, renombralo al copiarlo como `firmware.bin`.

### Crear el bundle simple

```powershell
Remove-Item -Recurse -Force .\firmware_bundle_simple -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path .\firmware_bundle_simple | Out-Null

Copy-Item .\app\build\bootloader\bootloader.bin .\firmware_bundle_simple\
Copy-Item .\app\build\partition_table\partition-table.bin .\firmware_bundle_simple\
Copy-Item .\app\build\main.bin .\firmware_bundle_simple\firmware.bin
```

Si el firmware viene de otra carpeta, reemplaza solo las rutas fuente:

```powershell
Copy-Item .\RUTA\DEL\BUILD\bootloader.bin .\firmware_bundle_simple\
Copy-Item .\RUTA\DEL\BUILD\partition-table.bin .\firmware_bundle_simple\
Copy-Item .\RUTA\DEL\BUILD\main.bin .\firmware_bundle_simple\firmware.bin
```

## Paso 2. Copiar scripts y bundle desde Windows a la Raspberry

Primero, asegurate de tener acceso por SSH a la Raspberry. Si vas a entrar por Tailscale, ese acceso debe estar resuelto antes de seguir.

Desde PowerShell, parado en la raiz del repo:

```powershell
cd C:\Users\mroch\Documents\Fiuba\i4a_project
```

### Copiar usando `scp`

Con hostname:

```powershell
scp -r .\firmware_bundle_simple i4a@Flasheador:~/i4a-flasher/
scp .\auxiliar\esp_rpi_flasher.py i4a@Flasheador:~/i4a-flasher/
scp .\auxiliar\rpi_esp_autoflash_daemon.py i4a@Flasheador:~/i4a-flasher/
scp .\auxiliar\install_rpi_autoflash.sh i4a@Flasheador:~/i4a-flasher/
scp .\auxiliar\i4a-rpi-autoflash.service i4a@Flasheador:~/i4a-flasher/
```

Con IP o destino Tailscale:

```powershell
scp -r .\firmware_bundle_simple i4a@<host-o-ip-tailscale>:~/i4a-flasher/
scp .\auxiliar\esp_rpi_flasher.py i4a@<host-o-ip-tailscale>:~/i4a-flasher/
scp .\auxiliar\rpi_esp_autoflash_daemon.py i4a@<host-o-ip-tailscale>:~/i4a-flasher/
scp .\auxiliar\install_rpi_autoflash.sh i4a@<host-o-ip-tailscale>:~/i4a-flasher/
scp .\auxiliar\i4a-rpi-autoflash.service i4a@<host-o-ip-tailscale>:~/i4a-flasher/
```

## Paso 3. Preparar la Raspberry

Entrar por SSH a la Raspberry:

```bash
ssh i4a@Flasheador
```

o usando el destino Tailscale que corresponda.

### Limpiar una instalacion vieja

Si queres evitar arrastrar archivos viejos:

```bash
rm -rf ~/i4a-flasher
mkdir -p ~/i4a-flasher
```

Si limpiaste antes de copiar, recopiá los archivos desde Windows despues de este paso.

### Verificar archivos copiados

```bash
ls -l ~/i4a-flasher
ls -l ~/i4a-flasher/firmware_bundle_simple
```

Deberias ver:

En `~/i4a-flasher`:

- `esp_rpi_flasher.py`
- `rpi_esp_autoflash_daemon.py`
- `install_rpi_autoflash.sh`
- `i4a-rpi-autoflash.service`

En `~/i4a-flasher/firmware_bundle_simple`:

- `bootloader.bin`
- `partition-table.bin`
- `firmware.bin`

## Paso 4. Crear entorno Python e instalar dependencias

```bash
sudo apt update
sudo apt install -y python3-venv python3-pip

cd ~/i4a-flasher
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install esptool pyserial
deactivate
```

Verificar que exista el Python del entorno:

```bash
ls -l /home/i4a/i4a-flasher/.venv/bin/python
```

## Paso 5. Dar permisos al instalador

```bash
chmod +x ~/i4a-flasher/install_rpi_autoflash.sh
```

Si el archivo vino con finales de linea Windows y falla al ejecutar:

```bash
sudo apt install -y dos2unix
dos2unix ~/i4a-flasher/install_rpi_autoflash.sh
```

## Paso 6. Prueba manual del daemon

Antes de instalar el servicio, conviene probarlo a mano:

```bash
sudo /home/i4a/i4a-flasher/.venv/bin/python /home/i4a/i4a-flasher/rpi_esp_autoflash_daemon.py \
  --bin-dir /home/i4a/i4a-flasher/firmware_bundle_simple \
  --app firmware.bin
```

Comportamiento esperado:

- sin ESP conectado: LED apagado
- conectas un ESP valido: LED titila
- si flashea bien: LED queda encendido mientras el ESP siga conectado
- al desconectar el ESP: LED se apaga

Para detener la prueba manual:

```bash
Ctrl+C
```

## Paso 7. Instalar el servicio

```bash
cd ~/i4a-flasher
sudo ./install_rpi_autoflash.sh \
  --bin-dir /home/i4a/i4a-flasher/firmware_bundle_simple \
  --app firmware.bin
```

Este instalador:

- genera `/etc/systemd/system/i4a-rpi-autoflash.service`
- recarga `systemd`
- habilita el servicio al boot
- reinicia el servicio

## Paso 8. Verificar estado y logs

### Estado del servicio

```bash
sudo systemctl status i4a-rpi-autoflash.service
```

### Logs en vivo

```bash
sudo journalctl -u i4a-rpi-autoflash.service -f
```

### Reiniciar el servicio

```bash
sudo systemctl restart i4a-rpi-autoflash.service
```

### Deshabilitarlo

```bash
sudo systemctl disable --now i4a-rpi-autoflash.service
```

## Paso 9. Probar que arranca solo al boot

```bash
sudo reboot
```

Cuando la Raspberry vuelva:

```bash
sudo systemctl status i4a-rpi-autoflash.service
sudo journalctl -u i4a-rpi-autoflash.service -f
```

## Problemas comunes

### `Unit i4a-rpi-autoflash.service could not be found`

Falta copiar `i4a-rpi-autoflash.service` a `~/i4a-flasher/` o no corriste el instalador.

### `No such file or directory: /home/i4a/i4a-flasher/.venv/bin/python`

El `venv` no existe o se borro. Rehacer el Paso 4 y luego:

```bash
sudo systemctl restart i4a-rpi-autoflash.service
```

### `Could not exclusively lock port /dev/ttyUSB0`

Otro proceso esta usando el puerto. Diagnostico:

```bash
sudo lsof /dev/ttyUSB0
sudo fuser -v /dev/ttyUSB0
```

### El instalador falla por finales de linea

```bash
sudo apt install -y dos2unix
dos2unix ~/i4a-flasher/install_rpi_autoflash.sh
```

## Mantenimiento cuando cambia el firmware

Cada vez que cambie el binario:

1. reemplaza `bootloader.bin`, `partition-table.bin` y `firmware.bin` en `firmware_bundle_simple`
2. vuelve a copiarlos a la Raspberry
3. reinicia el servicio:

```bash
sudo systemctl restart i4a-rpi-autoflash.service
```

## Checklist minimo

### En Windows

1. tener `bootloader.bin`, `partition-table.bin` y `firmware.bin`
2. armar `firmware_bundle_simple`
3. copiar bundle y scripts a la Raspberry por `scp`
4. entrar por SSH, incluyendo la variante por Tailscale si corresponde

### En Raspberry

1. crear `.venv`
2. instalar `esptool` y `pyserial`
3. ejecutar la prueba manual
4. correr `install_rpi_autoflash.sh`
5. revisar `systemctl status`
6. abrir `journalctl -f`
