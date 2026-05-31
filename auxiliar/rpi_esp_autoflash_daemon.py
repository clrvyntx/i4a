#!/usr/bin/env python3
"""
Daemon de auto-flash para Raspberry Pi.

Comportamiento:
- Detecta /dev/ttyUSB* y /dev/ttyACM*.
- Valida si el dispositivo responde como Espressif usando `esptool read-mac`.
- Si valida, flashea automaticamente usando `esp_rpi_flasher.py`.
- LED:
  - apagado: no hay ESP conectado
  - titilando: flasheo en progreso
  - encendido: ultimo flasheo exitoso y el ESP sigue conectado
"""

from __future__ import annotations

import argparse
import atexit
import pathlib
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from enum import Enum


SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
FLASHER_SCRIPT = SCRIPT_DIR / "esp_rpi_flasher.py"
LED_SYSFS_ROOT = pathlib.Path("/sys/class/leds")
PREFERRED_LED_NAMES = ("ACT", "led0", "act")


class PortStatus(str, Enum):
    NON_ESP = "non_esp"
    FAILED = "failed"
    FLASHED = "flashed"


class DaemonError(RuntimeError):
    pass


def log(message: str) -> None:
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] {message}", flush=True)


def run_command(command: list[str], timeout: float | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def parse_trigger_value(raw_value: str) -> str:
    for token in raw_value.strip().split():
        if token.startswith("[") and token.endswith("]"):
            return token[1:-1]
    return raw_value.strip().split()[0] if raw_value.strip() else "none"


class StatusLed:
    def __init__(self, requested_led: str | None) -> None:
        self.led_path = self._detect_led(requested_led)
        self.trigger_path: pathlib.Path | None = None
        self.brightness_path: pathlib.Path | None = None
        self.original_trigger: str | None = None
        self._blink_thread: threading.Thread | None = None
        self._blink_stop = threading.Event()
        self._lock = threading.Lock()

        if self.led_path is None:
            log("No se detecto un LED utilizable. El daemon sigue sin senializacion visual.")
            return

        self.trigger_path = self.led_path / "trigger"
        self.brightness_path = self.led_path / "brightness"

        try:
            if self.trigger_path.exists():
                self.original_trigger = parse_trigger_value(self.trigger_path.read_text(encoding="utf-8"))
                self.trigger_path.write_text("none\n", encoding="utf-8")
            self.off()
            log(f"Usando LED: {self.led_path.name}")
        except PermissionError as exc:
            raise DaemonError(
                f"Sin permisos para controlar el LED {self.led_path}. Ejecuta el daemon como root."
            ) from exc

        atexit.register(self.restore)

    def _detect_led(self, requested_led: str | None) -> pathlib.Path | None:
        if not LED_SYSFS_ROOT.is_dir():
            return None

        if requested_led and requested_led.lower() != "auto":
            candidate = LED_SYSFS_ROOT / requested_led
            if candidate.exists():
                return candidate
            raise DaemonError(
                f"El LED solicitado '{requested_led}' no existe. Disponibles: "
                f"{', '.join(sorted(p.name for p in LED_SYSFS_ROOT.iterdir()))}"
            )

        children = {path.name: path for path in LED_SYSFS_ROOT.iterdir()}

        for name in PREFERRED_LED_NAMES:
            if name in children:
                return children[name]

        for path in children.values():
            lowered = path.name.lower()
            if "act" in lowered or lowered == "led0":
                return path

        return None

    def _write_brightness(self, value: int) -> None:
        if self.brightness_path is None:
            return
        self.brightness_path.write_text(f"{value}\n", encoding="utf-8")

    def on(self) -> None:
        with self._lock:
            self.stop_blink()
            self._write_brightness(1)

    def off(self) -> None:
        with self._lock:
            self.stop_blink()
            self._write_brightness(0)

    def start_blink(self, interval_seconds: float = 0.30) -> None:
        if self.brightness_path is None:
            return

        with self._lock:
            self.stop_blink()
            self._blink_stop.clear()
            self._blink_thread = threading.Thread(
                target=self._blink_loop,
                args=(interval_seconds,),
                name="status-led-blink",
                daemon=True,
            )
            self._blink_thread.start()

    def stop_blink(self) -> None:
        if self._blink_thread is None:
            return

        self._blink_stop.set()
        self._blink_thread.join(timeout=2)
        self._blink_thread = None

    def _blink_loop(self, interval_seconds: float) -> None:
        level = 0
        while not self._blink_stop.is_set():
            level = 0 if level else 1
            try:
                self._write_brightness(level)
            except OSError:
                return
            self._blink_stop.wait(interval_seconds)
        self._write_brightness(0)

    def restore(self) -> None:
        if self.trigger_path is None:
            return

        try:
            self.stop_blink()
            self._write_brightness(0)
            if self.original_trigger:
                self.trigger_path.write_text(f"{self.original_trigger}\n", encoding="utf-8")
        except OSError:
            pass


@dataclass
class FlashConfig:
    chip: str
    flash_baud: str
    flash_args: pathlib.Path | None
    bin_dir: pathlib.Path | None
    app: str | None
    app_only: bool
    erase_first: bool


def ensure_file(path: pathlib.Path, label: str) -> pathlib.Path:
    if not path.is_file():
        raise DaemonError(f"No se encontro {label}: {path}")
    return path


def ensure_dir(path: pathlib.Path, label: str) -> pathlib.Path:
    if not path.is_dir():
        raise DaemonError(f"No se encontro {label}: {path}")
    return path


def build_flash_command(port: str, config: FlashConfig) -> list[str]:
    ensure_file(FLASHER_SCRIPT, "esp_rpi_flasher.py")

    command = [
        sys.executable,
        str(FLASHER_SCRIPT),
        "flash",
        "--port",
        port,
        "--chip",
        config.chip,
        "--baud",
        config.flash_baud,
    ]

    if config.erase_first:
        command.append("--erase-first")

    if config.flash_args is not None:
        command.extend(["--flash-args", str(config.flash_args)])
    else:
        if config.bin_dir is None:
            raise DaemonError("Falta --bin-dir o --flash-args")
        command.extend(["--bin-dir", str(config.bin_dir)])
        if config.app:
            command.extend(["--app", config.app])
        if config.app_only:
            command.append("--app-only")

    return command


def probe_esp_port(port: str, probe_timeout_seconds: float) -> bool:
    command = [
        sys.executable,
        "-m",
        "esptool",
        "--port",
        port,
        "read-mac",
    ]
    result = run_command(command, timeout=probe_timeout_seconds)
    if result.returncode == 0:
        summary = result.stdout.strip().splitlines()
        if summary:
            log(f"{port}: validado como ESP ({summary[-1]})")
        else:
            log(f"{port}: validado como ESP")
        return True

    stderr = result.stderr.strip() or result.stdout.strip() or "sin detalle"
    log(f"{port}: no responde como ESP ({stderr})")
    return False


def flash_port(port: str, config: FlashConfig, flash_timeout_seconds: float) -> bool:
    command = build_flash_command(port, config)
    log(f"{port}: iniciando flasheo")
    result = run_command(command, timeout=flash_timeout_seconds)

    if result.returncode == 0:
        log(f"{port}: flasheo OK")
        return True

    stderr = result.stderr.strip() or result.stdout.strip() or "sin detalle"
    log(f"{port}: flasheo fallido ({stderr})")
    return False


def list_serial_ports() -> list[str]:
    ports = sorted({str(path) for pattern in ("/dev/ttyUSB*", "/dev/ttyACM*") for path in pathlib.Path("/dev").glob(pattern[5:])})
    return ports


def any_flashed_port_connected(states: dict[str, PortStatus], current_ports: set[str]) -> bool:
    for port, status in states.items():
        if status == PortStatus.FLASHED and port in current_ports:
            return True
    return False


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Daemon de auto-flash para Raspberry Pi + ESP32."
    )
    parser.add_argument("--chip", default="esp32", help="Chip objetivo para el flasheo. Default: esp32")
    parser.add_argument("--flash-baud", default="921600", help="Baudrate de flasheo. Default: 921600")
    parser.add_argument("--probe-timeout", type=float, default=15.0, help="Timeout de validacion por puerto.")
    parser.add_argument("--flash-timeout", type=float, default=180.0, help="Timeout total de flasheo por puerto.")
    parser.add_argument("--settle-seconds", type=float, default=2.0, help="Espera tras detectar un puerto nuevo.")
    parser.add_argument("--poll-interval", type=float, default=1.0, help="Intervalo de polling.")
    parser.add_argument(
        "--led-name",
        default="auto",
        help="Nombre del LED en /sys/class/leds. Default: auto (prefiere ACT/led0).",
    )
    parser.add_argument("--flash-args", help="Ruta al archivo flash_args del bundle.")
    parser.add_argument("--bin-dir", help="Ruta al bundle simple.")
    parser.add_argument("--app", help="Nombre del binario de app cuando se usa --bin-dir.")
    parser.add_argument("--app-only", action="store_true", help="Flashea solo la app.")
    parser.add_argument("--erase-first", action="store_true", help="Hace erase_flash antes de escribir.")
    return parser


def validate_args(args: argparse.Namespace) -> FlashConfig:
    if bool(args.flash_args) == bool(args.bin_dir):
        raise DaemonError("Debes indicar exactamente uno entre --flash-args y --bin-dir")

    flash_args = pathlib.Path(args.flash_args).expanduser().resolve() if args.flash_args else None
    bin_dir = pathlib.Path(args.bin_dir).expanduser().resolve() if args.bin_dir else None

    if flash_args is not None:
        ensure_file(flash_args, "flash_args")
    if bin_dir is not None:
        ensure_dir(bin_dir, "bundle de binarios")

    return FlashConfig(
        chip=args.chip,
        flash_baud=args.flash_baud,
        flash_args=flash_args,
        bin_dir=bin_dir,
        app=args.app,
        app_only=args.app_only,
        erase_first=args.erase_first,
    )


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        config = validate_args(args)
        led = StatusLed(args.led_name)
    except DaemonError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    port_states: dict[str, PortStatus] = {}

    log("Daemon de auto-flash iniciado")
    while True:
        current_ports = set(list_serial_ports())

        removed_ports = set(port_states) - current_ports
        for port in sorted(removed_ports):
            log(f"{port}: desconectado")
            del port_states[port]

        new_ports = sorted(current_ports - set(port_states))
        for port in new_ports:
            log(f"{port}: detectado")
            time.sleep(args.settle_seconds)

            if port not in set(list_serial_ports()):
                log(f"{port}: desaparecio antes de validar")
                continue

            if not probe_esp_port(port, args.probe_timeout):
                port_states[port] = PortStatus.NON_ESP
                continue

            led.start_blink()
            flashed_ok = False
            try:
                flashed_ok = flash_port(port, config, args.flash_timeout)
            finally:
                led.stop_blink()

            port_states[port] = PortStatus.FLASHED if flashed_ok else PortStatus.FAILED

        if any_flashed_port_connected(port_states, current_ports):
            led.on()
        else:
            led.off()

        time.sleep(args.poll_interval)


if __name__ == "__main__":
    sys.exit(main())
