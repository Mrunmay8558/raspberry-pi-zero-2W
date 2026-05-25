#include "audio_device.h"

#include "daily_rtvi.h"

#include <atomic>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define PI_SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define PI_SLEEP_MS(ms) usleep((ms) * 1000)
#endif

namespace {

constexpr const char* kDailyConnectPath = "/v1/public/daily";

/** Runtime options required to connect this device to the hosted Daily session endpoint. */
struct CommandLineOptions {
    std::string connect_url;
    std::string config_file;
    std::string api_key;
};

/**
 * Top-level Raspberry Pi voice client application.
 *
 * The app configures Pipecat's native Daily client, passes the hosted eigi
 * endpoint and `X-API-Key` header into the SDK, and starts local audio only
 * after the Daily transport reports a successful connection.
 */
class VoiceClientApp : public rtvi::RTVIEventCallbacks {
   public:
    /**
     * Builds a Daily voice client from endpoint, request payload, and API key.
     *
     * @param connect_url Hosted endpoint that creates/returns Daily room details.
     * @param request_payload JSON body sent to the hosted endpoint.
     * @param api_key eigi API key used as the `X-API-Key` request header.
     */
    VoiceClientApp(
            const std::string& connect_url,
            const nlohmann::json& request_payload,
            const std::string& api_key
    )
        : _running(true) {
        std::vector<std::string> headers;
        if (!api_key.empty()) {
            headers.push_back("X-API-Key: " + api_key);
        }

        rtvi::RTVIClientEndpoints endpoints;
        endpoints.connect = connect_url;

        rtvi::RTVIClientParams params;
        params.endpoints = endpoints;
        params.request = request_payload;
        params.headers = headers;

        rtvi::RTVIClientOptions options;
        options.params = params;
        options.callbacks = this;

        _client = std::make_unique<rtvi::DailyVoiceClient>(options);
        _audio = std::make_unique<pi_voice_device::AudioDevice>(_client.get());
    }

    /** Connects to Daily and blocks until a signal, error, or disconnect stops the app. */
    void run() {
        _client->initialize();
        _client->connect();

        while (_running) {
            PI_SLEEP_MS(1000);
        }

        _client->disconnect();
    }

    /** Requests a graceful shutdown from a signal handler or callback. */
    void stop() {
        _running = false;
    }

    /** Starts local audio after the Daily transport is ready. */
    void on_connected() override {
        std::cout << "Connected to Daily transport" << std::endl;
        _audio->start();
    }

    /** Stops local audio when the Daily transport disconnects. */
    void on_disconnected() override {
        std::cout << "Disconnected from Daily transport" << std::endl;
        _audio->stop();
        _running = false;
    }

    /** Logs server-side errors and exits the run loop. */
    void on_error(const nlohmann::json& error) override {
        std::cerr << "Server error: " << error.dump() << std::endl;
        _running = false;
    }

    void on_user_started_speaking() override {
        std::cout << "User started speaking" << std::endl;
    }

    void on_user_stopped_speaking() override {
        std::cout << "User stopped speaking" << std::endl;
    }

    void on_bot_connected(const nlohmann::json& bot) override {
        std::cout << "Bot connected: " << bot.dump() << std::endl;
    }

    void on_bot_disconnected(const nlohmann::json& bot, const std::string& reason) override {
        std::cout << "Bot disconnected: " << reason << " " << bot.dump() << std::endl;
    }

    void on_bot_ready() override {
        std::cout << "Bot ready" << std::endl;
    }

    void on_bot_started_speaking() override {
        std::cout << "Bot started speaking" << std::endl;
    }

    void on_bot_stopped_speaking() override {
        std::cout << "Bot stopped speaking" << std::endl;
    }

   private:
    std::atomic<bool> _running;
    std::unique_ptr<rtvi::RTVIClient> _client;
    std::unique_ptr<pi_voice_device::AudioDevice> _audio;
};

std::unique_ptr<VoiceClientApp> app;

/** Handles SIGINT/SIGTERM by asking the app to leave the Daily session cleanly. */
void signal_handler(int signum) {
    std::cout << "Received shutdown signal: " << signum << std::endl;
    if (app) {
        app->stop();
    }
}

/** Prints command-line usage and the environment variables accepted by the app. */
void print_usage() {
    std::cout << "Usage: pi_voice_daily_client [--api-base-url URL] [--connect-url URL] "
                 "[--config FILE] [--api-key KEY]"
              << std::endl;
    std::cout << "Environment defaults: EIGI_API_BASE_URL, EIGI_PUBLIC_API_KEY, "
                 "PI_DEVICE_SESSION_PAYLOAD"
              << std::endl;
}

/** Returns an environment variable value or an empty string when it is unset. */
std::string get_env_or_empty(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
}

/** Removes trailing slashes so appending a fixed path does not produce double slashes. */
std::string trim_trailing_slashes(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

/** Builds the Daily session connect URL from the configured eigi API base URL. */
std::string build_daily_connect_url(const std::string& api_base_url) {
    if (api_base_url.empty()) {
        return std::string();
    }
    return trim_trailing_slashes(api_base_url) + kDailyConnectPath;
}

/**
 * Parses runtime configuration from environment variables and CLI overrides.
 *
 * Environment variables provide the normal `systemd` path. Command-line flags
 * are useful during local development because they can override any value from
 * `.env` without editing files.
 */
CommandLineOptions parse_args(int argc, char* argv[]) {
    CommandLineOptions options;
    std::string api_base_url = get_env_or_empty("EIGI_API_BASE_URL");
    options.connect_url = get_env_or_empty("PI_DEVICE_CONNECT_URL");
    options.config_file = get_env_or_empty("PI_DEVICE_SESSION_PAYLOAD");
    options.api_key = get_env_or_empty("EIGI_PUBLIC_API_KEY");

    if (options.api_key.empty()) {
        options.api_key = get_env_or_empty("PI_DEVICE_EIGI_API_KEY");
    }

    for (int index = 1; index < argc; ++index) {
        std::string arg = argv[index];
        if (arg == "--api-base-url" && index + 1 < argc) {
            api_base_url = argv[++index];
        } else if (arg == "--connect-url" && index + 1 < argc) {
            options.connect_url = argv[++index];
        } else if (arg == "--config" && index + 1 < argc) {
            options.config_file = argv[++index];
        } else if (arg == "--api-key" && index + 1 < argc) {
            options.api_key = argv[++index];
        } else {
            print_usage();
            throw std::runtime_error("Invalid command-line arguments");
        }
    }

    if (options.connect_url.empty()) {
        options.connect_url = build_daily_connect_url(api_base_url);
    }

    if (options.connect_url.empty() || options.config_file.empty() || options.api_key.empty()) {
        print_usage();
        throw std::runtime_error("Missing required command-line arguments");
    }

    return options;
}

/** Reads the JSON request payload that will be posted to the hosted endpoint. */
nlohmann::json read_json_file(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open config file: " + path);
    }

    nlohmann::json payload;
    input >> payload;
    return payload;
}

/**
 * Mirrors the top-level agent id into the bootstrap conversation metadata.
 *
 * The session creation endpoint expects the conversation bootstrap data in the
 * top-level `conversation_metadata` object. Default the conversation type to
 * `VOICE` so the native client starts the expected runtime path.
 */
nlohmann::json with_conversation_metadata(nlohmann::json payload) {
    if (!payload.contains("agent_id") || !payload["agent_id"].is_string()) {
        throw std::runtime_error("Session payload must include a string agent_id");
    }

    const std::string agent_id = payload["agent_id"].get<std::string>();

    nlohmann::json& conversation_metadata = payload["conversation_metadata"];
    if (!conversation_metadata.is_object()) {
        conversation_metadata = nlohmann::json::object();
    }

    conversation_metadata["agent_id"] = agent_id;

    if (!payload.contains("conversation_config_type") ||
        !payload["conversation_config_type"].is_string() ||
        payload["conversation_config_type"].get<std::string>().empty()) {
        payload["conversation_config_type"] = "VOICE";
    }

    return payload;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const CommandLineOptions options = parse_args(argc, argv);
        const nlohmann::json request_payload =
            with_conversation_metadata(read_json_file(options.config_file));

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        app = std::make_unique<VoiceClientApp>(
            options.connect_url,
            request_payload,
            options.api_key
        );
        app->run();
    } catch (const std::exception& exc) {
        std::cerr << "Fatal error: " << exc.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
