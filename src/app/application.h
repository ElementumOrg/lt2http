#pragma once

#include <cassert>
#include <csignal>
#include <atomic>

#include <oatpp/network/Server.hpp>

#include <app/config.h>
#include <app/app_component.h>

#include <bittorrent/session.h>

#include <web/error_handler.h>

void signalHandler(int signum);

namespace lh {

class Application
{
public:
    static void init(int argc, char *argv[])
    {
        OATPP_LOGI("Application::init", "Starting lt2http, version: %s, Libtorrent version: %s", lh::VERSION.c_str(), LIBTORRENT_VERSION)

        // The shared library can be started again in the same process, so state of a previous run is dropped.
        destroy();
        lh::is_closing.store(false);
        _inst = new Application{argc, argv};
    }

    static void destroy() {
        delete _inst;
        _inst = nullptr;
    }

    static bool initialized() {
        return _inst != nullptr;
    }

    static Application &get() {
        assert(_inst);
        return *_inst;
    }

    Application(Application const&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application const&) = delete;
    Application& operator=(Application &&) = delete;

    void run();
    void close();
    lh::Config& config();
    lh::Session& session();

    static Application* _inst;

private:
    lh::Config m_config;
    lh::Session m_session;

protected:
    Application(int &argc, char *argv[]);
    ~Application();
};

inline Application &app() {
    return lh::Application::get();
}

inline Config &config() {
    return lh::Application::get().config();
}

inline Session &session() {
    return lh::Application::get().session();
}

inline void set_close(bool val = true) {
    lh::is_closing.store(val);
}

}
