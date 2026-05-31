#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
  set +u
  # shellcheck disable=SC1090
  source "${IDF_PATH}/export.sh" >/dev/null
  set -u
elif [[ -f "/opt/esp/idf/export.sh" ]]; then
  set +u
  # shellcheck disable=SC1091
  source "/opt/esp/idf/export.sh" >/dev/null
  set -u
fi

cd "${I4A_APP_DIR:-/workspace/app}"

if [[ $# -eq 0 ]]; then
  exec bash
fi

exec "$@"
