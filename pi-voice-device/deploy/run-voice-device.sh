#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
APP_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
ENV_FILE=${PI_DEVICE_ENV_FILE:-"${APP_DIR}/.env"}
BINARY=${PI_DEVICE_BINARY:-"${APP_DIR}/native/pipecat_daily_client/build/pi_voice_daily_client"}

if [ ! -f "${ENV_FILE}" ]; then
  echo "Environment file not found: ${ENV_FILE}" >&2
  echo "Create ${APP_DIR}/.env from .env.local, .env.prod, or .env.example first." >&2
  exit 1
fi

if [ ! -x "${BINARY}" ]; then
  echo "Native binary not found or not executable: ${BINARY}" >&2
  echo "Build native/pipecat_daily_client before starting the device." >&2
  exit 1
fi

set -a
. "${ENV_FILE}"
set +a

if [ -z "${EIGI_API_BASE_URL:-}" ]; then
  echo "EIGI_API_BASE_URL is not set in ${ENV_FILE}" >&2
  exit 1
fi

if [ -z "${EIGI_PUBLIC_API_KEY:-}" ]; then
  echo "EIGI_PUBLIC_API_KEY is not set in ${ENV_FILE}" >&2
  exit 1
fi

if [ -z "${PI_DEVICE_SESSION_PAYLOAD:-}" ]; then
  echo "PI_DEVICE_SESSION_PAYLOAD is not set in ${ENV_FILE}" >&2
  exit 1
fi

cd "${APP_DIR}"

if [ ! -f "${PI_DEVICE_SESSION_PAYLOAD}" ]; then
  echo "Session payload file not found: ${PI_DEVICE_SESSION_PAYLOAD}" >&2
  exit 1
fi

exec "${BINARY}" "$@"