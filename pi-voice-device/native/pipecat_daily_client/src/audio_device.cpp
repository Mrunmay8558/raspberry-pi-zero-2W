#include "audio_device.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#define PI_SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define PI_SLEEP_MS(ms) usleep((ms) * 1000)
#endif

namespace pi_voice_device {
namespace {

/** Converts a PortAudio error code into a readable exception. */
std::runtime_error portaudio_error(const std::string& context, PaError error) {
    return std::runtime_error(context + ": " + Pa_GetErrorText(error));
}

}  // namespace

AudioInput::AudioInput(rtvi::RTVIClient* client, uint32_t sample_rate)
    : _recording(false), _client(client), _sample_rate(sample_rate), _stream(nullptr) {}

AudioInput::~AudioInput() {
    if (_recording) {
        stop();
    }
}

void AudioInput::start() {
    // One input channel, no output channel, signed 16-bit PCM.
    PaError error = Pa_OpenDefaultStream(
            &_stream,
            1,
            0,
            paInt16,
            _sample_rate,
            paFramesPerBufferUnspecified,
            &AudioInput::on_portaudio_input,
            this
    );

    if (error != paNoError) {
        throw portaudio_error("Unable to open audio input", error);
    }

    error = Pa_StartStream(_stream);
    if (error != paNoError) {
        Pa_CloseStream(_stream);
        _stream = nullptr;
        throw portaudio_error("Unable to start audio input", error);
    }

    _recording = true;
}

void AudioInput::stop() {
    // PortAudio permits stop/close during normal shutdown; ignore close status
    // here because the device is already leaving the session.
    _recording = false;
    if (_stream != nullptr) {
        Pa_StopStream(_stream);
        Pa_CloseStream(_stream);
        _stream = nullptr;
    }
}

int AudioInput::on_portaudio_input(
        const void* input_buffer,
        void* output_buffer,
        unsigned long frame_count,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags,
        void* user_data
) {
    (void)output_buffer;
    (void)time_info;
    (void)status_flags;
    return static_cast<AudioInput*>(user_data)->handle_audio(input_buffer, frame_count);
}

int AudioInput::handle_audio(const void* input_buffer, unsigned long frame_count) {
    if (input_buffer != nullptr) {
        // Pipecat expects interleaved signed 16-bit PCM frames.
        _client->send_user_audio(static_cast<const int16_t*>(input_buffer), frame_count);
    }
    return paContinue;
}

AudioOutput::AudioOutput(rtvi::RTVIClient* client, uint32_t sample_rate)
    : _started(false),
      _playing(false),
      _client(client),
      _sample_rate(sample_rate),
      _stream(nullptr) {}

AudioOutput::~AudioOutput() {
    if (_started) {
        stop();
    }
}

void AudioOutput::start() {
    // No input channel, one output channel, signed 16-bit PCM.
    PaError error = Pa_OpenDefaultStream(
            &_stream,
            0,
            1,
            paInt16,
            _sample_rate,
            paFramesPerBufferUnspecified,
            &AudioOutput::on_portaudio_output,
            this
    );

    if (error != paNoError) {
        throw portaudio_error("Unable to open audio output", error);
    }

    error = Pa_StartStream(_stream);
    if (error != paNoError) {
        Pa_CloseStream(_stream);
        _stream = nullptr;
        throw portaudio_error("Unable to start audio output", error);
    }

    _started = true;
    _read_thread = std::thread(&AudioOutput::read_transport_audio, this);
}

void AudioOutput::stop() {
    _started = false;
    if (_read_thread.joinable()) {
        _read_thread.join();
    }
    if (_stream != nullptr) {
        Pa_StopStream(_stream);
        Pa_CloseStream(_stream);
        _stream = nullptr;
    }
}

void AudioOutput::read_transport_audio() {
    // At 16 kHz, 160 frames is roughly 10 ms of mono audio.
    constexpr size_t frame_count = 160;
    auto frames = std::make_unique<int16_t[]>(frame_count);

    while (_started) {
        size_t received = _client->read_bot_audio(frames.get(), frame_count);
        if (received > 0) {
            append_audio(frames.get(), received);
        } else {
            PI_SLEEP_MS(1);
        }
    }
}

void AudioOutput::append_audio(const int16_t* frames, size_t frame_count) {
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    _buffer.insert(_buffer.end(), frames, frames + frame_count);
}

int AudioOutput::on_portaudio_output(
        const void* input_buffer,
        void* output_buffer,
        unsigned long frame_count,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags,
        void* user_data
) {
    (void)input_buffer;
    (void)time_info;
    (void)status_flags;
    return static_cast<AudioOutput*>(user_data)->write_audio(output_buffer, frame_count);
}

int AudioOutput::write_audio(void* output_buffer, unsigned long frame_count) {
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    auto* output = static_cast<int16_t*>(output_buffer);

    if (!_playing) {
        // Wait for a small amount of queued audio before playback starts. This
        // reduces underruns on tiny devices where callback timing can wobble.
        const size_t minimum_start_frames = std::max<size_t>(frame_count * 2, 320);
        if (_buffer.size() < minimum_start_frames) {
            std::memset(output, 0, frame_count * sizeof(int16_t));
            return paContinue;
        }
        _playing = true;
    }

    const size_t frames_to_copy = std::min<size_t>(frame_count, _buffer.size());
    std::copy(_buffer.begin(), _buffer.begin() + frames_to_copy, output);
    _buffer.erase(_buffer.begin(), _buffer.begin() + frames_to_copy);

    if (frames_to_copy < frame_count) {
        // If the assistant has not produced enough audio, finish the block with
        // silence rather than replaying old samples or leaving garbage bytes.
        std::memset(
                output + frames_to_copy,
                0,
                (frame_count - frames_to_copy) * sizeof(int16_t)
        );
        _playing = false;
    }

    return paContinue;
}

AudioDevice::AudioDevice(rtvi::RTVIClient* client) {
    // PortAudio is process-global, so initialize it once before opening streams.
    PaError error = Pa_Initialize();
    if (error != paNoError) {
        throw portaudio_error("Unable to initialize PortAudio", error);
    }

    _input = std::make_unique<AudioInput>(client, 16000);
    _output = std::make_unique<AudioOutput>(client, 16000);
}

AudioDevice::~AudioDevice() {
    stop();
    _input.reset();
    _output.reset();
    Pa_Terminate();
}

void AudioDevice::start() {
    _input->start();
    _output->start();
}

void AudioDevice::stop() {
    _input->stop();
    _output->stop();
}

}  // namespace pi_voice_device
