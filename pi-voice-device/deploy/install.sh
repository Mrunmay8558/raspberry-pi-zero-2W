#!/usr/bin/env sh
set -eu

APP_DIR="/opt/pi-voice-device"
SERVICE_FILE="/etc/systemd/system/voice-device.service"
SESSION_PAYLOAD="${APP_DIR}/config/session_payload.json"
NATIVE_BINARY="${APP_DIR}/native/pipecat_daily_client/build/pi_voice_daily_client"
RUN_SCRIPT="${APP_DIR}/deploy/run-voice-device.sh"
SERVICE_USER="${SUDO_USER:-${USER}}"

echo "Installing pi voice device into ${APP_DIR}"
sudo mkdir -p "${APP_DIR}"
sudo cp -R . "${APP_DIR}"

if [ ! -f "${APP_DIR}/.env" ]; then
  if [ -f "${APP_DIR}/.env.prod" ]; then
    sudo cp "${APP_DIR}/.env.prod" "${APP_DIR}/.env"
  else
    sudo cp "${APP_DIR}/.env.example" "${APP_DIR}/.env"
  fi
  echo "Created ${APP_DIR}/.env. Edit it before starting the service."
fi

if [ ! -f "${SESSION_PAYLOAD}" ]; then
  sudo cp "${APP_DIR}/config/session_payload.example.json" "${SESSION_PAYLOAD}"
  echo "Created ${SESSION_PAYLOAD}. Edit it for your eigi Daily session request."
fi

if [ ! -x "${NATIVE_BINARY}" ]; then
  echo "Native binary not found at ${NATIVE_BINARY}."
  echo "Build native/pipecat_daily_client before starting the service."
fi

sudo chmod +x "${RUN_SCRIPT}"

if ! id "${SERVICE_USER}" >/dev/null 2>&1; then
  echo "Service user does not exist: ${SERVICE_USER}" >&2
  exit 1
fi

sed "s/__PI_DEVICE_USER__/${SERVICE_USER}/g" "${APP_DIR}/deploy/voice-device.service" | sudo tee "${SERVICE_FILE}" >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable voice-device.service

echo "Install complete."
echo "Run once manually with: ${RUN_SCRIPT}"
echo "Run as a service with: sudo systemctl start voice-device.service"
