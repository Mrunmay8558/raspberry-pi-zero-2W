#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
APP_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${APP_DIR}/native/pipecat_daily_client/build"
DEPS_DIR="${APP_DIR}/.deps"
DAILY_CORE_VERSION="${DAILY_CORE_VERSION:-0.20.0}"
DAILY_CORE_ARCHIVE="daily-core-sdk-${DAILY_CORE_VERSION}-linux-arm64.zip"
DAILY_CORE_URL="https://github.com/daily-co/daily-core-sdk/releases/download/v${DAILY_CORE_VERSION}/${DAILY_CORE_ARCHIVE}"

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

ensure_env_file() {
  if [ -f "${APP_DIR}/.env" ]; then
    return
  fi

  if [ -f "${APP_DIR}/.env.prod" ]; then
    cp "${APP_DIR}/.env.prod" "${APP_DIR}/.env"
  else
    cp "${APP_DIR}/.env.example" "${APP_DIR}/.env"
  fi

  echo "Created ${APP_DIR}/.env"
}

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

set_env_var() {
  file_path="$1"
  var_name="$2"
  var_value="$3"
  escaped_value=$(printf '%s\n' "$var_value" | sed 's/[\\&]/\\&/g')

  if grep -q "^${var_name}=" "$file_path"; then
    sed -i "s#^${var_name}=.*#${var_name}=${escaped_value}#" "$file_path"
  else
    printf '\n%s=%s\n' "$var_name" "$var_value" >> "$file_path"
  fi
}

set_installed_env_var() {
  file_path="$1"
  var_name="$2"
  var_value="$3"

  sudo FILE_PATH="$file_path" VAR_NAME="$var_name" VAR_VALUE="$var_value" sh -c '
    escaped_value=$(printf "%s\n" "$VAR_VALUE" | sed "s/[\\&]/\\\\&/g")

    if grep -q "^${VAR_NAME}=" "$FILE_PATH"; then
      sed -i "s#^${VAR_NAME}=.*#${VAR_NAME}=${escaped_value}#" "$FILE_PATH"
    else
      printf "\n%s=%s\n" "$VAR_NAME" "$VAR_VALUE" >> "$FILE_PATH"
    fi
  '
}

clone_repo_if_missing() {
  repo_url="$1"
  checkout_path="$2"

  if [ -d "${checkout_path}/.git" ]; then
    return
  fi

  mkdir -p "$(dirname "${checkout_path}")"
  git clone --depth 1 "$repo_url" "$checkout_path"
}

ensure_daily_pipecat_compat() {
  transport_header="${DAILY_PIPECAT_SDK_PATH}/include/daily_transport.h"

  if [ ! -f "${transport_header}" ]; then
    return
  fi

  sed -i 's/NativeDeviceManager\*/DailyDeviceManager*/g' "${transport_header}"
}

ensure_daily_core_sdk() {
  if [ -n "${DAILY_CORE_PATH:-}" ]; then
    require_env_path DAILY_CORE_PATH
    return
  fi

  DAILY_CORE_PATH="${DEPS_DIR}/daily-core-sdk-${DAILY_CORE_VERSION}-linux-arm64"
  export DAILY_CORE_PATH

  if [ -f "${DAILY_CORE_PATH}/include/daily_core.h" ]; then
    return
  fi

  mkdir -p "${DEPS_DIR}"
  archive_path="${DEPS_DIR}/${DAILY_CORE_ARCHIVE}"

  echo "Downloading Daily Core SDK ${DAILY_CORE_VERSION}"
  curl -fL "${DAILY_CORE_URL}" -o "${archive_path}"
  unzip -qo "${archive_path}" -d "${DEPS_DIR}"

  require_env_path DAILY_CORE_PATH
}

ensure_pipecat_sdk() {
  if [ -z "${PIPECAT_SDK_PATH:-}" ]; then
    PIPECAT_SDK_PATH="${DEPS_DIR}/pipecat-client-cxx"
    export PIPECAT_SDK_PATH
    clone_repo_if_missing "https://github.com/pipecat-ai/pipecat-client-cxx.git" "${PIPECAT_SDK_PATH}"
  fi

  require_env_path PIPECAT_SDK_PATH

  if [ -f "${PIPECAT_SDK_PATH}/lib/Release/libpipecat.a" ] || [ -f "${PIPECAT_SDK_PATH}/lib/libpipecat.a" ]; then
    return
  fi

  echo "Building Pipecat C++ SDK"
  cd "${PIPECAT_SDK_PATH}"
  cmake . -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
  ninja -C build
}

ensure_daily_pipecat_sdk() {
  if [ -z "${DAILY_PIPECAT_SDK_PATH:-}" ]; then
    DAILY_PIPECAT_SDK_PATH="${DEPS_DIR}/pipecat-client-cxx-daily"
    export DAILY_PIPECAT_SDK_PATH
    clone_repo_if_missing "https://github.com/pipecat-ai/pipecat-client-cxx-daily.git" "${DAILY_PIPECAT_SDK_PATH}"
  fi

  require_env_path DAILY_PIPECAT_SDK_PATH
  ensure_daily_pipecat_compat

  if [ -f "${DAILY_PIPECAT_SDK_PATH}/lib/Release/libdaily_pipecat.a" ] || [ -f "${DAILY_PIPECAT_SDK_PATH}/lib/libdaily_pipecat.a" ]; then
    return
  fi

  echo "Building Pipecat Daily transport SDK"
  cd "${DAILY_PIPECAT_SDK_PATH}"
  cmake . -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
  ninja -C build
}

build_runtime_library_path() {
  runtime_paths=""

  for candidate in \
    "${DAILY_CORE_PATH}/lib" \
    "${DAILY_CORE_PATH}/lib/Release" \
    "${DAILY_PIPECAT_SDK_PATH}/lib" \
    "${DAILY_PIPECAT_SDK_PATH}/lib/Release" \
    "${PIPECAT_SDK_PATH}/lib" \
    "${PIPECAT_SDK_PATH}/lib/Release"; do
    if [ -d "${candidate}" ]; then
      if [ -z "${runtime_paths}" ]; then
        runtime_paths="${candidate}"
      else
        runtime_paths="${runtime_paths}:${candidate}"
      fi
    fi
  done

  printf '%s\n' "${runtime_paths}"
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
  portaudio19-dev \
  unzip

require_command cmake
require_command ninja

ensure_env_file
require_command curl
require_command git
require_command unzip

ensure_daily_core_sdk
ensure_pipecat_sdk
ensure_daily_pipecat_sdk

LD_LIBRARY_PATH_VALUE=$(build_runtime_library_path)
if [ -n "${LD_LIBRARY_PATH_VALUE}" ]; then
  set_env_var "${APP_DIR}/.env" LD_LIBRARY_PATH "${LD_LIBRARY_PATH_VALUE}"
fi

echo "Building native Pi voice client"
mkdir -p "${BUILD_DIR}"
cd "${APP_DIR}/native/pipecat_daily_client"
cmake . -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build pi_voice_daily_client

cd "${APP_DIR}"
"${SCRIPT_DIR}/install.sh"

if [ -n "${LD_LIBRARY_PATH_VALUE}" ] && [ -f "/opt/pi-voice-device/.env" ]; then
  set_installed_env_var "/opt/pi-voice-device/.env" LD_LIBRARY_PATH "${LD_LIBRARY_PATH_VALUE}"
fi

echo "Setup complete. Start the bot with:"
echo "  /opt/pi-voice-device/deploy/run-voice-device.sh"
echo "or"
echo "  sudo systemctl start voice-device.service"