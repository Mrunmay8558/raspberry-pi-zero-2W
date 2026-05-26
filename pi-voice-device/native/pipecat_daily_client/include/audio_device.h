#ifndef PI_VOICE_DEVICE_AUDIO_DEVICE_H
#define PI_VOICE_DEVICE_AUDIO_DEVICE_H

#include "rtvi.h"

#include <portaudio.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace pi_voice_device {

/**
 * Captures microphone audio from the configured or default PortAudio input device.
 *
 * PortAudio calls this class on a real-time audio callback thread. The callback
 * forwards 16-bit PCM microphone frames into the Pipecat RTVI client, which
 * sends them to the Daily transport.
 */
class AudioInput {
   public:
    /**
     * Creates a microphone input stream wrapper.
     *
     * @param client Active RTVI client that receives captured user audio.
     * @param sample_rate Audio sample rate requested from PortAudio.
     */
    AudioInput(rtvi::RTVIClient* client, uint32_t sample_rate);
    ~AudioInput();

    /** Opens and starts the configured or default microphone input stream. */
    void start();

    /** Stops and closes the microphone input stream if it is running. */
    void stop();

   private:
    /** Static PortAudio callback that forwards input frames to this instance. */
    static int on_portaudio_input(
            const void* input_buffer,
            void* output_buffer,
            unsigned long frame_count,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags status_flags,
            void* user_data
    );

    /** Sends one PortAudio input block into the RTVI client. */
    int handle_audio(const void* input_buffer, unsigned long frame_count);

    std::atomic<bool> _recording;
    rtvi::RTVIClient* _client;
    uint32_t _sample_rate;
    PaStream* _stream;
};

/**
 * Plays assistant audio received from the Pipecat RTVI client.
 *
 * A background thread reads bot audio from the Daily transport and stores it in
 * a small PCM buffer. PortAudio drains that buffer on its output callback
 * thread and writes silence whenever no assistant audio is available.
 */
class AudioOutput {
   public:
    /**
     * Creates a speaker output stream wrapper.
     *
     * @param client Active RTVI client that provides bot audio frames.
     * @param sample_rate Audio sample rate requested from PortAudio.
     */
    AudioOutput(rtvi::RTVIClient* client, uint32_t sample_rate);
    ~AudioOutput();

    /** Opens the configured or default speaker output stream and starts the read thread. */
    void start();

    /** Stops the read thread and closes the speaker output stream. */
    void stop();

   private:
    /** Continuously reads assistant audio frames from the RTVI client. */
    void read_transport_audio();

    /** Appends received assistant audio frames to the playback buffer. */
    void append_audio(const int16_t* frames, size_t frame_count);

    /** Static PortAudio callback that forwards output requests to this instance. */
    static int on_portaudio_output(
            const void* input_buffer,
            void* output_buffer,
            unsigned long frame_count,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags status_flags,
            void* user_data
    );

    /** Fills one PortAudio output block from the buffered assistant audio. */
    int write_audio(void* output_buffer, unsigned long frame_count);

    std::atomic<bool> _started;
    bool _playing;
    rtvi::RTVIClient* _client;
    uint32_t _sample_rate;
    PaStream* _stream;
    std::thread _read_thread;
    std::mutex _buffer_mutex;
    std::deque<int16_t> _buffer;
};

/**
 * Owns the complete local audio device lifecycle.
 *
 * This class initializes PortAudio once, creates matching microphone and
 * speaker streams, and terminates PortAudio after both streams have stopped.
 */
class AudioDevice {
   public:
    /** Creates input/output streams connected to the provided RTVI client. */
    explicit AudioDevice(rtvi::RTVIClient* client);
    ~AudioDevice();

    /** Starts microphone capture and speaker playback. */
    void start();

    /** Stops microphone capture and speaker playback. */
    void stop();

   private:
    std::unique_ptr<AudioInput> _input;
    std::unique_ptr<AudioOutput> _output;
};

}  // namespace pi_voice_device

#endif
