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
    /** Applies this component's configured ESP-IDF log level. */
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

        /** Restarts the server using the latest configuration. */
        esp_err_t reset();

        /** Returns whether the HTTPS server is running. */
        bool isInitialized() const;

        /** Returns the non-owning ConfigStore dependency. */
        app::cfg::ConfigStore &getConfigStore();

        /** Returns the non-owning StateMachine dependency. */
        app::sm::StateMachine &getStateMachine();

    private:
        bool isInitialized_ = false;
        app::cfg::ConfigStore &cfgStore_;
        app::sm::StateMachine &stateMachine_;
        struct State;
        std::unique_ptr<State> state_;
    };
}
