#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_TEMPLATE="${SCRIPT_DIR}/i4a-rpi-autoflash.service"
SERVICE_TARGET="/etc/systemd/system/i4a-rpi-autoflash.service"
WORKDIR_DEFAULT="/home/i4a/i4a-flasher"
PYTHON_DEFAULT="${WORKDIR_DEFAULT}/.venv/bin/python"
DAEMON_DEFAULT="${WORKDIR_DEFAULT}/rpi_esp_autoflash_daemon.py"

usage() {
  cat <<'EOF'
Uso:
  sudo ./install_rpi_autoflash.sh --flash-args /home/i4a/i4a-flasher/firmware_bundle_idf/flash_args

O:
  sudo ./install_rpi_autoflash.sh --bin-dir /home/i4a/i4a-flasher/firmware_bundle_simple --app firmware.bin

Opcionales:
  --workdir PATH
  --python PATH
  --daemon PATH
  --chip esp32
  --flash-baud 921600
  --led-name auto
  --app-only
  --erase-first
EOF
}

die() {
  echo "Error: $*" >&2
  exit 1
}

require_arg() {
  local key="$1"
  local value="${2:-}"
  [[ -n "${value}" ]] || die "Falta valor para ${key}"
}

workdir="${WORKDIR_DEFAULT}"
python_bin="${PYTHON_DEFAULT}"
daemon_path="${DAEMON_DEFAULT}"
chip="esp32"
flash_baud="921600"
led_name="auto"
flash_args=""
bin_dir=""
app_name=""
app_only="0"
erase_first="0"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workdir)
      require_arg "$1" "${2:-}"
      workdir="$2"
      shift 2
      ;;
    --python)
      require_arg "$1" "${2:-}"
      python_bin="$2"
      shift 2
      ;;
    --daemon)
      require_arg "$1" "${2:-}"
      daemon_path="$2"
      shift 2
      ;;
    --flash-args)
      require_arg "$1" "${2:-}"
      flash_args="$2"
      shift 2
      ;;
    --bin-dir)
      require_arg "$1" "${2:-}"
      bin_dir="$2"
      shift 2
      ;;
    --app)
      require_arg "$1" "${2:-}"
      app_name="$2"
      shift 2
      ;;
    --chip)
      require_arg "$1" "${2:-}"
      chip="$2"
      shift 2
      ;;
    --flash-baud)
      require_arg "$1" "${2:-}"
      flash_baud="$2"
      shift 2
      ;;
    --led-name)
      require_arg "$1" "${2:-}"
      led_name="$2"
      shift 2
      ;;
    --app-only)
      app_only="1"
      shift
      ;;
    --erase-first)
      erase_first="1"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "Argumento no reconocido: $1"
      ;;
  esac
done

if [[ -n "${flash_args}" && -n "${bin_dir}" ]]; then
  die "Usa solo uno entre --flash-args y --bin-dir"
fi

if [[ -z "${flash_args}" && -z "${bin_dir}" ]]; then
  die "Debes indicar --flash-args o --bin-dir"
fi

[[ -x "${python_bin}" ]] || die "No existe el Python del venv: ${python_bin}"
[[ -f "${daemon_path}" ]] || die "No existe el daemon: ${daemon_path}"
[[ -f "${SERVICE_TEMPLATE}" ]] || die "No existe la plantilla de servicio: ${SERVICE_TEMPLATE}"

exec_start="${python_bin} ${daemon_path} --chip ${chip} --flash-baud ${flash_baud} --led-name ${led_name}"
if [[ -n "${flash_args}" ]]; then
  exec_start="${exec_start} --flash-args ${flash_args}"
else
  exec_start="${exec_start} --bin-dir ${bin_dir}"
  if [[ -n "${app_name}" ]]; then
    exec_start="${exec_start} --app ${app_name}"
  fi
  if [[ "${app_only}" == "1" ]]; then
    exec_start="${exec_start} --app-only"
  fi
fi

if [[ "${erase_first}" == "1" ]]; then
  exec_start="${exec_start} --erase-first"
fi

tmp_service="$(mktemp)"
trap 'rm -f "${tmp_service}"' EXIT

cat > "${tmp_service}" <<EOF
[Unit]
Description=I4A Raspberry Pi ESP Auto Flasher
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${workdir}
ExecStart=${exec_start}
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

install -m 0644 "${tmp_service}" "${SERVICE_TARGET}"
systemctl daemon-reload
systemctl enable i4a-rpi-autoflash.service
systemctl restart i4a-rpi-autoflash.service

echo "Servicio instalado en ${SERVICE_TARGET}"
echo "Estado:"
systemctl --no-pager --full status i4a-rpi-autoflash.service || true
