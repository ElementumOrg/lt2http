#pragma once

#include <cassert>
#include <atomic>

#include <oatpp/network/Server.hpp>

#include <app/config.h>
#include <app/app_component.h>

#include <bittorrent/session.h>

#include <web/error_handler.h>

namespace lh {

class Application
{
public:
    static void init(int argc, char *argv[])
    {
        OATPP_LOGI("Application::init", "Initializing application");
        static Application instance{argc, argv};
        _inst = &instance;
    }

    static Application &get() {
        assert(_inst);
        return *_inst;
    }

    Application(Application const&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application const&) = delete;
    Application& operator=(Application &&) = delete;

    void run() const;
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

static Application &app() {
    return lh::Application::get();
}

static Config &config() {
    return lh::Application::get().config();
}

static Session &session() {
    return lh::Application::get().session();
}

static void set_close(bool val = true) {
    lh::is_closing.store(val);
};

}
