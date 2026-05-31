#!/usr/bin/env python3
"""
CLI simple para flashear ESP32 desde una Raspberry Pi usando esptool.

Soporta dos modos:
1. Modo bundle simple: bootloader + partition table + app con offsets estandar.
2. Modo flash_args: reutiliza el archivo `flash_args` generado por ESP-IDF.
"""

from __future__ import annotations

import argparse
import glob
import pathlib
import shlex
import subprocess
import sys
from typing import Iterable, List, Sequence, Tuple


DEFAULT_BOOTLOADER_OFFSET = "0x1000"
DEFAULT_PARTITION_OFFSET = "0x8000"
DEFAULT_APP_OFFSET = "0x10000"
DEFAULT_FLASH_BAUD = "921600"
DEFAULT_MONITOR_BAUD = "115200"

KNOWN_BOOTLOADER_NAMES = ("bootloader.bin",)
KNOWN_PARTITION_NAMES = ("partition-table.bin", "partitions.bin")
IGNORED_APP_BIN_NAMES = {
    "bootloader.bin",
    "partition-table.bin",
    "partitions.bin",
    "ota_data_initial.bin",
}


class FlasherError(RuntimeError):
    pass


def list_serial_ports() -> List[str]:
    ports = sorted(set(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")))
    return ports


def require_existing_file(path: pathlib.Path, label: str) -> pathlib.Path:
    if not path.is_file():
        raise FlasherError(f"No se encontro {label}: {path}")
    return path


def detect_named_file(bin_dir: pathlib.Path, candidates: Iterable[str], label: str) -> pathlib.Path:
    for name in candidates:
        candidate = bin_dir / name
        if candidate.is_file():
            return candidate
    raise FlasherError(
        f"No se encontro {label} en {bin_dir}. Nombres esperados: {', '.join(candidates)}"
    )


def detect_app_binary(bin_dir: pathlib.Path, explicit_app: str | None) -> pathlib.Path:
    if explicit_app:
        return require_existing_file(bin_dir / explicit_app, "binario de aplicacion")

    preferred = bin_dir / "firmware.bin"
    if preferred.is_file():
        return preferred

    candidates = [
        path for path in sorted(bin_dir.glob("*.bin")) if path.name not in IGNORED_APP_BIN_NAMES
    ]

    if not candidates:
        raise FlasherError(
            "No se encontro el binario de aplicacion. Usa --app o agrega firmware.bin al bundle."
        )

    if len(candidates) > 1:
        names = ", ".join(path.name for path in candidates)
        raise FlasherError(
            "Hay mas de un .bin candidato para la aplicacion. Usa --app para elegir uno: "
            f"{names}"
        )

    return candidates[0]


def strip_managed_global_options(tokens: Sequence[str]) -> List[str]:
    filtered: List[str] = []
    skip_next = False

    for index, token in enumerate(tokens):
        if skip_next:
            skip_next = False
            continue

        if token in {"--chip", "--port", "--baud", "-p", "-b"}:
            if index + 1 >= len(tokens):
                raise FlasherError(f"flash_args invalido: falta valor para {token}")
            skip_next = True
            continue

        filtered.append(token)

    return filtered


def parse_flash_args_tokens(
    flash_args_path: pathlib.Path,
) -> Tuple[List[str], List[str], List[Tuple[str, pathlib.Path]]]:
    raw_tokens = shlex.split(flash_args_path.read_text(encoding="utf-8"))
    before_write_tokens: List[str] = []
    write_option_tokens: List[str] = []
    segments: List[Tuple[str, pathlib.Path]] = []
    seen_write_flash = "write_flash" not in raw_tokens

    i = 0
    while i < len(raw_tokens):
        token = raw_tokens[i]

        if token == "write_flash":
            seen_write_flash = True
            i += 1
            continue

        if token.startswith("0x"):
            if i + 1 >= len(raw_tokens):
                raise FlasherError(f"flash_args invalido: falta archivo despues de {token}")
            image_path = (flash_args_path.parent / raw_tokens[i + 1]).resolve()
            segments.append((token, image_path))
            i += 2
            continue

        if seen_write_flash:
            write_option_tokens.append(token)
        else:
            before_write_tokens.append(token)
        i += 1

    if not segments:
        raise FlasherError(f"No se encontraron segmentos de flash en {flash_args_path}")

    before_write_tokens = strip_managed_global_options(before_write_tokens)

    return before_write_tokens, write_option_tokens, segments


def build_flash_segments_from_bundle(
    bin_dir: pathlib.Path,
    app_name: str | None,
    app_only: bool,
    bootloader_offset: str,
    partition_offset: str,
    app_offset: str,
) -> List[Tuple[str, pathlib.Path]]:
    app_bin = detect_app_binary(bin_dir, app_name)

    if app_only:
        return [(app_offset, app_bin)]

    bootloader = detect_named_file(bin_dir, KNOWN_BOOTLOADER_NAMES, "bootloader")
    partition_table = detect_named_file(bin_dir, KNOWN_PARTITION_NAMES, "partition table")

    return [
        (bootloader_offset, bootloader),
        (partition_offset, partition_table),
        (app_offset, app_bin),
    ]


def resolve_ports(requested_ports: Sequence[str], all_detected: bool) -> List[str]:
    ports: List[str] = []

    if requested_ports:
        ports.extend(requested_ports)

    if all_detected:
        detected = list_serial_ports()
        if not detected:
            raise FlasherError("No se detectaron puertos serie para --all-detected")
        ports.extend(detected)

    ports = sorted(dict.fromkeys(ports))

    if not ports:
        raise FlasherError("Debes indicar al menos un --port o usar --all-detected")

    return ports


def run_command(command: Sequence[str]) -> None:
    subprocess.run(command, check=True)


def ensure_pyserial_for_monitor() -> None:
    try:
        import serial  # noqa: F401
    except ImportError as exc:
        raise FlasherError(
            "pyserial no esta instalado. Instala con: python3 -m pip install pyserial"
        ) from exc


def command_list(_args: argparse.Namespace) -> int:
    ports = list_serial_ports()
    if not ports:
        print("No se detectaron puertos /dev/ttyUSB* ni /dev/ttyACM*.")
        return 1

    for port in ports:
        print(port)
    return 0


def command_flash(args: argparse.Namespace) -> int:
    ports = resolve_ports(args.port, args.all_detected)

    if args.flash_args:
        flash_args_path = require_existing_file(pathlib.Path(args.flash_args).resolve(), "flash_args")
        before_write_tokens, write_option_tokens, segments = parse_flash_args_tokens(flash_args_path)
    else:
        bin_dir = pathlib.Path(args.bin_dir).resolve() if args.bin_dir else None
        if not bin_dir:
            raise FlasherError("Debes indicar --bin-dir cuando no usas --flash-args")
        if not bin_dir.is_dir():
            raise FlasherError(f"No existe la carpeta de binarios: {bin_dir}")

        before_write_tokens = []
        write_option_tokens = []
        segments = build_flash_segments_from_bundle(
            bin_dir=bin_dir,
            app_name=args.app,
            app_only=args.app_only,
            bootloader_offset=args.bootloader_offset,
            partition_offset=args.partition_offset,
            app_offset=args.app_offset,
        )

    for _, image_path in segments:
        require_existing_file(image_path, "imagen de flash")

    for port in ports:
        print(f"[flash] Puerto: {port}")

        if args.erase_first:
            erase_cmd = [
                sys.executable,
                "-m",
                "esptool",
                "--chip",
                args.chip,
                "--port",
                port,
                "--baud",
                args.baud,
                "erase_flash",
            ]
            print("[flash] Ejecutando erase_flash...")
            run_command(erase_cmd)

        write_cmd = [
            sys.executable,
            "-m",
            "esptool",
            "--chip",
            args.chip,
            "--port",
            port,
            "--baud",
            args.baud,
        ]
        write_cmd.extend(before_write_tokens)
        write_cmd.extend([
            "write_flash",
        ])

        if "-z" not in write_option_tokens and "--compress" not in write_option_tokens:
            write_cmd.append("-z")

        write_cmd.extend(write_option_tokens)

        for offset, image_path in segments:
            write_cmd.extend([offset, str(image_path)])

        run_command(write_cmd)
        print(f"[flash] OK en {port}")

    return 0


def command_monitor(args: argparse.Namespace) -> int:
    ensure_pyserial_for_monitor()
    command = [
        sys.executable,
        "-m",
        "serial.tools.miniterm",
        args.port,
        args.baud,
    ]
    run_command(command)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Flasheador ESP32 para Raspberry Pi usando esptool."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list", help="Lista puertos serie detectados")
    list_parser.set_defaults(handler=command_list)

    flash_parser = subparsers.add_parser("flash", help="Flashea uno o mas ESP32")
    flash_parser.add_argument(
        "--port",
        action="append",
        default=[],
        help="Puerto serie. Repetible. Ej: --port /dev/ttyUSB0 --port /dev/ttyUSB1",
    )
    flash_parser.add_argument(
        "--all-detected",
        action="store_true",
        help="Flashea todos los /dev/ttyUSB* y /dev/ttyACM* detectados",
    )
    flash_parser.add_argument("--chip", default="esp32", help="Chip objetivo. Default: esp32")
    flash_parser.add_argument(
        "--baud",
        default=DEFAULT_FLASH_BAUD,
        help=f"Baudrate de flasheo. Default: {DEFAULT_FLASH_BAUD}",
    )
    flash_parser.add_argument(
        "--erase-first",
        action="store_true",
        help="Ejecuta erase_flash antes del write_flash",
    )
    flash_parser.add_argument(
        "--flash-args",
        help="Ruta al archivo flash_args generado por ESP-IDF. Si se usa, ignora --bin-dir y offsets.",
    )
    flash_parser.add_argument(
        "--bin-dir",
        help="Carpeta con los .bin a flashear",
    )
    flash_parser.add_argument(
        "--app",
        help="Nombre del binario de aplicacion dentro de --bin-dir. Si se omite, se autodetecta.",
    )
    flash_parser.add_argument(
        "--app-only",
        action="store_true",
        help="Flashea solo la aplicacion en el offset de app. No escribe bootloader ni partition table.",
    )
    flash_parser.add_argument(
        "--bootloader-offset",
        default=DEFAULT_BOOTLOADER_OFFSET,
        help=f"Offset del bootloader. Default: {DEFAULT_BOOTLOADER_OFFSET}",
    )
    flash_parser.add_argument(
        "--partition-offset",
        default=DEFAULT_PARTITION_OFFSET,
        help=f"Offset de partition table. Default: {DEFAULT_PARTITION_OFFSET}",
    )
    flash_parser.add_argument(
        "--app-offset",
        default=DEFAULT_APP_OFFSET,
        help=f"Offset de la aplicacion. Default: {DEFAULT_APP_OFFSET}",
    )
    flash_parser.set_defaults(handler=command_flash)

    monitor_parser = subparsers.add_parser("monitor", help="Abre monitor serie con miniterm")
    monitor_parser.add_argument("--port", required=True, help="Puerto serie. Ej: /dev/ttyUSB0")
    monitor_parser.add_argument(
        "--baud",
        default=DEFAULT_MONITOR_BAUD,
        help=f"Baudrate del monitor. Default: {DEFAULT_MONITOR_BAUD}",
    )
    monitor_parser.set_defaults(handler=command_monitor)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        return args.handler(args)
    except FlasherError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        print(f"Error: el comando termino con codigo {exc.returncode}", file=sys.stderr)
        return exc.returncode


if __name__ == "__main__":
    sys.exit(main())
