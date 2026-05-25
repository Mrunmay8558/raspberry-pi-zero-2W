#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
APP_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${APP_DIR}/native/pipecat_daily_client/build"

require_command() {
  command_name="$1"

  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required command not found: ${command_name}" >&2
    exit 1
  fi
}

if [ "$(uname -s)" != "Linux" ]; then
  echo "setup-pi.sh must be run on Raspberry Pi OS or another Linux system." >&2
  exit 1
fi

require_command sudo
require_command apt-get
require_command systemctl
require_command cmake
require_command ninja

require_env_path() {
  var_name="$1"
  var_value=$(printenv "$var_name" || true)

  if [ -z "${var_value}" ]; then
    echo "${var_name} is required." >&2
    echo "Export ${var_name} before running setup-pi.sh." >&2
    exit 1
  fi

  if [ ! -d "${var_value}" ]; then
    echo "${var_name} does not point to a directory: ${var_value}" >&2
    exit 1
  fi
}

echo "Installing Raspberry Pi system packages"
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  curl \
  git \
  libcurl4-openssl-dev \
  libportaudio2 \
  ninja-build \
  pkg-config \
  portaudio19-dev

require_env_path PIPECAT_SDK_PATH
require_env_path DAILY_PIPECAT_SDK_PATH
require_env_path DAILY_CORE_PATH

echo "Building native Pi voice client"
mkdir -p "${BUILD_DIR}"
cd "${APP_DIR}/native/pipecat_daily_client"
cmake . -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build pi_voice_daily_client

cd "${APP_DIR}"
"${SCRIPT_DIR}/install.sh"

echo "Setup complete. Start the bot with:"
echo "  /opt/pi-voice-device/deploy/run-voice-device.sh"
echo "or"
echo "  sudo systemctl start voice-device.service"