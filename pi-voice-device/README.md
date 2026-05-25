# Pi Voice Device

`pi-voice-device` is the Raspberry Pi native client for the voice device MVP.

The MVP does not need a local Python bridge. The native Pipecat C++ Daily
client can call the hosted eigi Daily endpoint directly, as long as the endpoint
returns the connection payload expected by the Pipecat Daily transport.

Runtime flow:

1. `systemd` or the launcher script starts the native C++ client.
2. The service loads one environment file at `/opt/pi-voice-device/.env`.
3. The client reads `EIGI_API_BASE_URL`, `EIGI_PUBLIC_API_KEY`, and
   `PI_DEVICE_SESSION_PAYLOAD`.
4. The client posts the JSON payload to `${EIGI_API_BASE_URL}/v1/public/daily`
   with `X-API-Key`.
5. The Pipecat Daily transport receives the Daily `room_url` and `token`, joins
   the room, then handles live microphone and speaker audio.

## Environment

Use `.env.local` for a local backend on port `4000`, or `.env.prod` for the
hosted production API. Copy one of them to `.env` before running the service:

```bash
cp .env.local .env
cp config/session_payload.example.json config/session_payload.json
```

The important values are:

- `EIGI_API_BASE_URL`: API server base URL, for example `http://localhost:4000`
  locally or `https://api.eigi.ai` in production
- `EIGI_PUBLIC_API_KEY`: your eigi public API key; keep it out of Git
- `PI_DEVICE_SESSION_PAYLOAD`: JSON request body file for the Daily session

## Native Build

Install Raspberry Pi OS Lite 64-bit packages:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build pkg-config libcurl4-openssl-dev libportaudio2 portaudio19-dev
```

Build the Pipecat C++ SDK, Pipecat Daily SDK, and Daily Core SDK, then export
their locations:

```bash
export PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
export DAILY_PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx-daily
export DAILY_CORE_PATH=/path/to/daily-core-sdk
```

Build the native client:

```bash
cd native/pipecat_daily_client
cmake . -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

If you want one Pi-side setup command that installs packages, builds the native
client, copies the app to `/opt/pi-voice-device`, and enables the service, use:

```bash
export PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
export DAILY_PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx-daily
export DAILY_CORE_PATH=/path/to/daily-core-sdk
./deploy/setup-pi.sh
```

`setup-pi.sh` installs the Raspberry Pi OS packages automatically, but it still
expects the Pipecat SDK, Pipecat Daily SDK, and Daily Core SDK to already be
present on disk at those paths.

## Run Locally

Use the launcher script so manual runs and `systemd` use the same entrypoint:

```bash
cp .env.local .env
./deploy/run-voice-device.sh
```

## Run On Raspberry Pi

Install the repo under `/opt/pi-voice-device`, then use the same launcher
script directly or through `systemd`:

```bash
cd /opt/pi-voice-device
./deploy/run-voice-device.sh
```

```bash
sudo systemctl start voice-device.service
sudo systemctl status voice-device.service
```

## Wake Word

There is no wake-word or hotword implementation in this repo right now.
The current device behavior is direct start: run the launcher script or start
the `voice-device.service`, and the bot connects immediately.
