# Raspberry Pi Zero 2 W Voice Device Plan

## Goal

Build a Raspberry Pi Zero 2 W voice device that behaves like a small Alexa-style terminal for eigi.ai.

The device should:

- connect to Wi-Fi
- join a Pipecat Daily session
- capture microphone audio
- play assistant audio
- support full-duplex conversation with interruption when possible
- authenticate safely without storing a master Daily API key on the device

The Raspberry Pi is the voice endpoint.

The eigi.ai platform remains the brain:

- transport/session setup
- speech recognition
- agent logic
- tool execution
- memory
- text-to-speech

## Current Decisions

## Device Model

Use Raspberry Pi Zero 2 W for MVP hardware.

Why:

- small form factor
- built-in 2.4 GHz Wi-Fi
- built-in Bluetooth 4.2
- enough compute for a thin voice client

Constraint:

- 512 MB RAM means the device should not run local STT, local LLM, or local TTS inference

## Transport

Use Pipecat with Daily WebRTC as the primary transport.

Reasoning:

- the existing voice system already uses Pipecat
- a public Daily endpoint already exists
- the target interaction is full-duplex with barge-in
- Pipecat Daily already handles real-time media, room participation, and transport events

WebSocket is still a valid fallback for an extremely minimal push-to-talk path, but it should not be the default direction for this device because it would duplicate transport work already handled by Pipecat + Daily.

## Client Runtime Choice

Use the Pipecat C++ Daily client as the default Raspberry Pi device client.

Reasoning:

- the official native Pipecat client path supports Linux `aarch64`, which matches a 64-bit Raspberry Pi OS build
- a native client is a better fit for an always-on `systemd` device service than a browser-style runtime
- the Raspberry Pi Zero 2 W has limited RAM, so a native client is a safer baseline than a JavaScript client plus browser runtime
- this keeps the Pi focused on microphone capture, audio playback, connection lifecycle, and local device state

JavaScript client status:

- JavaScript is acceptable only if the Raspberry Pi is intentionally running a browser-based or kiosk-style UI
- JavaScript is not the default recommendation for a headless Raspberry Pi voice appliance
- Node.js without a browser is not the preferred MVP path for mic and speaker handling on this hardware

Implementation consequence:

- the Raspberry Pi should use the Pipecat C++ Daily client for the live media connection
- for the MVP, no Python bridge is required if eigi's hosted Daily endpoint returns the Pipecat-compatible connection payload
- the C++ client should call the hosted endpoint directly with `X-API-Key` from the device `.env` file

## Security Model

Do not put the master Daily API key on the Raspberry Pi.

Use this flow instead:

1. Device boots and authenticates to eigi.ai with a device credential.
2. eigi.ai uses the Daily API key on the backend.
3. eigi.ai creates or authorizes the session.
4. eigi.ai returns a short-lived room token and room URL.
5. The device joins Daily using the short-lived token.

This keeps Daily account control on the server side.

## MVP Credential Shortcut

For a personal or tightly controlled MVP, the Raspberry Pi may call the eigi.ai public API directly using the user's own eigi API key.

This is acceptable only as an explicit trust decision for MVP use, not as the preferred production model.

If this shortcut is used:

- the API key must not be committed to Git or baked into the device image
- the key should be stored only in local device configuration or environment variables
- the key should be treated as revocable account access, not as a low-risk secret
- if the device is lost, copied, or compromised, the user must revoke and replace the key

Production direction:

- the Raspberry Pi should authenticate to a backend owned by eigi.ai or the customer
- that backend should call eigi.ai using the protected platform API key
- the Raspberry Pi should receive only the short-lived Daily session details needed to join

## Target User Experience

The desired interaction model is:

- user speaks naturally
- assistant can speak back immediately
- user can interrupt the assistant mid-response
- assistant audio playback stops or ducks quickly on user speech
- conversation continues in the same session until timeout or disconnect

This is effectively a full-duplex voice assistant with barge-in.

## System Architecture

```text
Raspberry Pi Zero 2 W
  mic capture
  speaker playback
  optional button / LED
  Daily client session
  reconnect + local state
        |
        v
Pipecat Daily room
        |
        v
eigi.ai / ikki.ai backend
  device auth
  room/token provisioning
  Pipecat bot pipeline
  STT
  agent logic
  tools + memory
  TTS
  session management
```

## Responsibilities

## On The Raspberry Pi

- join Wi-Fi
- run an always-on device service
- fetch short-lived Daily session details from eigi.ai
- connect to the Pipecat Daily session
- capture mic audio
- play assistant audio
- track local device state
- reconnect automatically after Wi-Fi or session drops
- expose simple status output through logs and optional LED/button state

## On eigi.ai

- authenticate devices
- store device registry and provisioning state
- use the Daily API key to create or authorize sessions
- return short-lived Daily room access details
- run the Pipecat voice pipeline
- manage STT, agent execution, tools, memory, and TTS
- enforce session timeout, rate limiting, and logging

## Hardware Requirements

## Required

- Raspberry Pi Zero 2 W
- microSD card
- reliable power supply
- Wi-Fi network with 2.4 GHz enabled
- microphone input
- audio output device
- micro USB OTG adapter

## Recommended For MVP

- USB headset with built-in microphone

Why this is the simplest MVP path:

- one device handles mic + speaker together
- avoids most echo problems during initial testing
- easiest to validate end-to-end behavior

Headset compatibility requirement:

- a valid MVP headset path is either a USB headset with built-in mic or a wired 3.5 mm headset used through a USB audio dongle that supports microphone input
- the Raspberry Pi Zero 2 W power port is not a data port; audio accessories must use the micro USB OTG/data port
- a passive micro USB to 3.5 mm jack adapter is not sufficient because the Pi needs a real USB audio device for analog headsets

## Alternatives

- USB microphone + USB speaker through OTG adapter and powered hub
- USB audio dongle with 3.5 mm headphone output and mic input
- I2S microphone and I2S DAC/amp for a more embedded product design

## AirPods

AirPods are acceptable for regular MVP testing, but not as the baseline production audio path.

Use them for:

- pairing validation
- audio playback testing
- microphone capture testing
- basic conversation flow testing

Do not rely on them to judge final production quality because Bluetooth adds extra variability:

- latency
- profile switching issues
- unstable full-duplex behavior on Linux
- harder debugging when audio routing fails

## Device Software Requirements

## Operating System

- Raspberry Pi OS Lite 64-bit

This keeps the runtime small and focused on the voice client.

## Local Runtime

Implement a small device agent/service with these responsibilities:

- startup and configuration loading
- Bluetooth or USB audio device selection
- direct eigi API authentication for MVP mode through `X-API-Key`
- room/token fetch
- Daily connection lifecycle
- microphone and speaker device handling
- local session control
- reconnect logic
- structured logs

Device runtime split:

- native C++ Pipecat Daily client handles the real-time media session
- the native client calls `POST /v1/public/daily` directly for the MVP
- a local or backend adapter is only needed later if the hosted endpoint stops returning the shape expected by the native Pipecat client

The service should run under `systemd` so the device behaves like an appliance rather than a manually launched script.

## Audio Requirements

## MVP Audio Targets

- single microphone input
- single output device
- reliable playback
- reliable mic capture
- stable round-trip conversation

## Full-Duplex Requirements

- continuous mic capture while assistant audio is playing
- fast detection of user speech during assistant playback
- immediate playback interruption or ducking on barge-in
- low enough end-to-end latency for natural interaction
- echo handling that avoids assistant self-triggering

## Important Reality Constraint

Transport is not the hardest problem once Daily is in place.

The hardest practical issues will be:

- audio routing
- acoustic echo
- microphone quality
- playback bleed into the mic
- reconnect behavior
- timing around interruption and cancellation

## Provisioning And Auth Plan

Each Raspberry Pi should be treated as a registered device.

Minimum model:

- `device_id`
- `device_secret` or provisioned token
- backend-issued short-lived access token
- backend-issued Daily room URL and Daily token

Provisioning flow:

1. Flash device image.
2. Connect device to Wi-Fi.
3. Enter or load device credential.
4. Device calls eigi.ai provisioning endpoint.
5. Backend validates the device.
6. Backend returns usable session credentials.

## Repository And Deployment Workflow

The Raspberry Pi device client should live in its own GitHub repository or in a clearly separated `device/` directory of a monorepo.

Recommended MVP workflow:

1. Write and test the device code on a development machine such as a Mac.
2. Push changes to the GitHub repository.
3. On the Raspberry Pi, clone the repository once during setup.
4. For updates, pull the latest changes on the Pi.
5. Restart the device service so the new code is running.

Typical Raspberry Pi deployment loop:

1. `git pull` the latest device code on the Pi.
2. install or update runtime dependencies
3. reload or restart the `systemd` service
4. verify logs and device state

Recommended repository contents:

- device application source code
- configuration template such as `.env.example`
- dependency manifest
- `systemd` service file
- setup or install script
- README with Raspberry Pi setup and recovery steps

Suggested device repository shape:

```text
pi-voice-device/
  src/
    main.py
    config.py
    audio.py
    eigi_client.py
    session.py
    state.py
  deploy/
    voice-device.service
    install.sh
  .env.example
  requirements.txt
  README.md
```

Recommended developer workflow:

- use VS Code locally and push to GitHub
- use SSH or VS Code Remote SSH to access the Raspberry Pi
- pull the repository on the Raspberry Pi instead of editing production files manually
- keep device-specific configuration on the Pi and outside version control

## End-To-End API And Session Flow

The Daily session creation call prepares the session. It does not itself carry live microphone audio.

The key session provisioning call is:

```bash
curl "${EIGI_API_BASE_URL}/v1/public/daily" \
  -H "X-API-Key: ${EIGI_PUBLIC_API_KEY}" \
  -H "Content-Type: application/json" \
  --data @config/session_payload.json
```

That call should be understood as a session creation or authorization step.

High-level runtime flow:

1. Raspberry Pi boots and the device service starts.
2. The device service loads local configuration.
3. The device service selects the microphone and speaker device.
4. The device service authenticates using one of the supported modes:
   - MVP mode: direct call to eigi.ai with the user's own eigi API key
   - production mode: call customer or platform backend first
5. The native C++ client calls `POST /v1/public/daily` with `X-API-Key` to create or authorize the Daily session.
6. eigi.ai returns the Daily room URL and short-lived join credentials.
7. The Pipecat C++ Daily client uses those returned details to initialize the transport.
8. The Pipecat client joins the Daily WebRTC session.
9. Microphone audio is captured on the Pi and sent into the session.
10. Assistant audio is received from the session and played on the Pi output device.
11. Reconnect logic requests fresh session details when needed.

Responsibility split:

- `POST /v1/public/daily` provisions or authorizes a session
- Pipecat C++ Daily client transport manages the live WebRTC connection
- Daily carries the real-time audio transport
- eigi.ai handles the agent pipeline, STT, tools, memory, and TTS

Native client compatibility note:

- the Pipecat C++ Daily client expects a connect endpoint that returns `room_url` and `token`
- for the MVP, `https://api.eigi.ai/v1/public/daily` should be that connect endpoint
- if `POST /v1/public/daily` returns a different response shape later, a backend adapter is preferred; a local adapter can still be added if needed

## Operating Modes

### MVP Mode

Use this mode for a personal, first-party, or tightly controlled device.

- Raspberry Pi stores the user's own eigi API key locally
- Raspberry Pi calls eigi.ai directly
- Raspberry Pi receives session details and joins Daily
- if the device is compromised, the key must be revoked and replaced

### Production Mode

Use this mode for shared, managed, or customer-deployed devices.

- Raspberry Pi stores only a device credential
- Raspberry Pi calls a trusted backend
- trusted backend calls eigi.ai using the protected API key
- trusted backend returns only the short-lived Daily join information to the device

## Implementation Phases

## Phase 1: Device Bootstrap

Goal: get the Pi online and manageable.

Tasks:

- install Raspberry Pi OS Lite
- configure Wi-Fi
- enable SSH for remote access
- update system packages
- verify Bluetooth and USB device visibility
- create a local service account if needed

Exit criteria:

- device boots reliably
- device joins Wi-Fi automatically
- device is reachable remotely

## Phase 2: Basic Audio Validation

Goal: verify the chosen audio hardware on the Pi.

Tasks:

- pair AirPods or attach USB headset
- verify output playback
- verify microphone capture
- confirm chosen audio device is stable across reboot
- document the preferred device naming and selection method

Exit criteria:

- playback works
- mic works
- device selection is repeatable after reboot

## Phase 3: Secure Session Join

Goal: connect the Pi to eigi.ai and Daily without exposing master credentials.

Tasks:

- define device auth endpoint
- define session/token endpoint
- keep Daily API key only on backend
- return short-lived room token + room URL to the device
- add token refresh or reconnect handling
- define whether the device is running in MVP direct-call mode or production backend-brokered mode

Exit criteria:

- Pi can request session credentials
- Pi can join the correct Daily room
- no master Daily API key is stored on the device
- the chosen credential mode is documented and working end to end

## Phase 4: Thin Pipecat Device Client

Goal: make the Pi a functioning real-time voice endpoint.

Tasks:

- start Daily client connection
- capture microphone audio into the session
- receive assistant audio from the session
- expose connection and error events in logs
- handle disconnects and reconnect cleanly
- verify that `POST /v1/public/daily` directly returns the `room_url` and `token` response expected by the Pipecat C++ Daily client

Exit criteria:

- user can talk to the bot through the Pi
- assistant audio plays back on the Pi
- reconnect path works after a transient network loss
- the native C++ client runs successfully on Raspberry Pi OS Lite 64-bit

## Phase 5: Barge-In And Full-Duplex Behavior

Goal: make the interaction feel assistant-like rather than turn-by-turn only.

Tasks:

- keep mic active during assistant playback
- detect user speech during playback
- stop or duck assistant audio on user interruption
- verify session state resets correctly after interruption
- tune timing so interruption feels immediate

Exit criteria:

- user can interrupt assistant speech
- interrupted assistant response cancels cleanly
- follow-up user turn is captured correctly

## Phase 6: Appliance Hardening

Goal: move from demo behavior to stable device behavior.

Tasks:

- run the device agent under `systemd`
- add boot-on-start behavior
- add health logging and restart policy
- add simple status indicator support if desired
- document field recovery steps
- document the GitHub pull and service restart update workflow

Exit criteria:

- power cycle returns the device to a working ready state
- service auto-recovers from common failures

## Suggested MVP Scope

The MVP should stop here:

- Raspberry Pi Zero 2 W
- Raspberry Pi OS Lite
- AirPods or preferably a USB headset for testing
- backend-issued Daily room token
- Pi joins Pipecat Daily session
- mic and speaker both work
- assistant speech plays back
- user speech reaches the bot
- interruption works at a basic level

Do not add in MVP unless required:

- wake word
- custom enclosure
- on-device STT
- on-device TTS
- local LLM inference
- advanced visual UI

## Risks And Mitigations

## Risk: Bluetooth audio instability

Mitigation:

- use AirPods only for testing
- switch to USB headset if behavior is inconsistent

## Risk: Echo and self-triggering

Mitigation:

- validate with a headset first
- only evaluate open-speaker mode after the transport path is proven

## Risk: Overloading the Zero 2 W

Mitigation:

- keep the Pi as a thin client
- keep all AI processing on eigi.ai

## Risk: Credential leakage

Mitigation:

- store master Daily API key only on backend
- issue short-lived session credentials to the device

## Acceptance Criteria

The first implementation is successful when all of the following are true:

- Pi boots and joins Wi-Fi automatically
- Pi authenticates to eigi.ai
- Pi receives a Daily room URL and short-lived token
- Pi joins the Pipecat Daily session
- user audio reaches the bot
- bot audio reaches the user
- session reconnect works after a temporary interruption
- user can interrupt assistant playback in a usable way

## Immediate Next Steps

1. Finalize the MVP audio hardware choice: AirPods for quick testing or USB headset for stable testing.
2. Use direct eigi API calls from the Raspberry Pi for the first MVP build.
3. Confirm `POST /v1/public/daily` returns `room_url` and `token` for the Pipecat client.
4. Create the Raspberry Pi device repository and initial deployment workflow using GitHub, SSH, and `systemd`.
5. Build the minimal Pi device service that authenticates, joins Daily, and routes mic and speaker audio.
6. Add barge-in handling and confirm full-duplex behavior under real device conditions.
