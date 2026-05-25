# Native Pipecat Daily Client

This directory contains the native Raspberry Pi media client. It calls the
hosted eigi Daily endpoint directly and handles the real-time audio path:

- captures microphone audio through PortAudio
- sends user audio to the Pipecat Daily transport
- reads bot audio from the transport
- plays assistant audio through PortAudio

## Expected Connect Endpoint

The client posts the configured JSON payload to the hosted eigi Daily endpoint:

```text
${EIGI_API_BASE_URL}/v1/public/daily
```

The request includes the eigi API key as an HTTP header:

```text
X-API-Key: vk_your_api_key_here
```

When the client starts a new conversation, it mirrors the top-level `agent_id`
into the outbound `conversation_metadata` block and defaults
`conversation_config_type` to `VOICE` when the payload does not set it.

The endpoint must return the shape expected by the Pipecat C++ Daily transport:

```json
{
  "room_url": "https://example.daily.co/room",
  "token": "daily-room-token"
}
```

## Dependencies

Install system packages on Raspberry Pi OS Lite 64-bit:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build pkg-config libcurl4-openssl-dev libportaudio2 portaudio19-dev
```

Build the Pipecat C++ SDK and Pipecat Daily transport SDK separately, then set:

```bash
export PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
export DAILY_PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx-daily
export DAILY_CORE_PATH=/path/to/daily-core-sdk
```

## Build

```bash
cmake . -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

## Run

```bash
export EIGI_API_BASE_URL=http://localhost:4000
export EIGI_PUBLIC_API_KEY=vk_your_api_key_here
export PI_DEVICE_SESSION_PAYLOAD=../../config/session_payload.example.json
./build/pi_voice_daily_client
```
