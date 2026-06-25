#pragma once

#include <memory>
#include "esp_err.h"

namespace app::cfg
{
    class ConfigStore;
}

namespace app::sm
{
    class StateMachine;
}

namespace app::https
{
    void apply_log_level();

    /**
     * Owns the HTTPS configuration/status server and registered URI handlers.
     * Referenced ConfigStore and StateMachine instances must outlive this object.
     */
    class HttpsServer
    {
    public:
        HttpsServer(app::cfg::ConfigStore &, app::sm::StateMachine &);

        ~HttpsServer();

        HttpsServer(const HttpsServer &) = delete;
        HttpsServer &operator=(const HttpsServer &) = delete;

        /** Starts the TLS server and registers all endpoints. Idempotent after success. */
        esp_err_t init();

        /** Stops the server and releases its ESP-IDF HTTP resources. */
        void fini();

        esp_err_t reset();

        bool isInitialized() const;

        app::cfg::ConfigStore &getConfigStore();

        app::sm::StateMachine &getStateMachine();

    private:
        bool isInitialized_ = false;
        app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;
        struct State;
        std::unique_ptr<State> state_;
    };
}
