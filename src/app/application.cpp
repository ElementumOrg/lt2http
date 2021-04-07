#include "application.h"

#include <iostream>
#include <memory>
#include <csignal>

#include <boost/stacktrace.hpp>

#include <app/config.h>

#include <web/files.h>
#include <web/misc.h>
#include <web/settings.h>
#include <web/session.h>
#include <web/stream.h>
#include <web/swagger.h>
#include <web/torrents.h>

void signalHandler( int signum ) {
    OATPP_LOGE("Application::signalHandler", "Got interrupt signal (%d). Stopping application gracefully", signum);
    lh::set_close(true);

    if (signum == SIGSEGV) {
        std::cerr << boost::stacktrace::stacktrace();

        // void *array[10];
        // size_t size;

        // // get void*'s for all entries on the stack
        // size = backtrace(array, 10);

        // // print out all the frames to stderr
        // backtrace_symbols_fd(array, size, STDERR_FILENO);
        // // exit(signum);
    };
}

namespace lh {

// Static
Application* Application::_inst = nullptr;

Application::Application(int &argc, char *argv[])
    : m_config(Config(argc, argv))
    , m_session(Session(m_config)) {}

Application::~Application() = default;

lh::Config& Application::config() {
    return m_config;
};

lh::Session& Application::session() {
    return m_session;
}

void Application::run() const {
    OATPP_LOGI("Application::run", "Starting lt2http, version: %s", lh::VERSION.c_str());

    std::signal(SIGINT, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGKILL, signalHandler);
    std::signal(SIGSEGV, signalHandler);

    lh::web_interface = m_config.web_interface;
    lh::web_port = m_config.web_port;

    OATPP_LOGI("Application::run", "Initializing Web Server on http://%s:%d", web_interface.c_str(), web_port);

    /* Register components in scope of run() */
    AppComponent components;

    auto router = components.httpRouter.getObject();
    auto docEndpoints = oatpp::swagger::CustomController::Endpoints::createShared();
  
    auto settingsController = SettingsController::createShared();
    settingsController->addEndpointsToRouter(router);

    auto miscController = MiscController::createShared();
    miscController->addEndpointsToRouter(router);

    auto sessionController = SessionController::createShared();
    sessionController->addEndpointsToRouter(router);

    auto torrentsController = TorrentsController::createShared();
    torrentsController->addEndpointsToRouter(router);

    auto filesController = FilesController::createShared();
    filesController->addEndpointsToRouter(router);

    auto streamController = StreamController::createShared();
    streamController->addEndpointsToRouter(router);

    docEndpoints->pushBackAll(settingsController->getEndpoints());
    docEndpoints->pushBackAll(miscController->getEndpoints());
    docEndpoints->pushBackAll(sessionController->getEndpoints());
    docEndpoints->pushBackAll(torrentsController->getEndpoints());
    docEndpoints->pushBackAll(filesController->getEndpoints());
    docEndpoints->pushBackAll(streamController->getEndpoints());
  
    auto swaggerController = oatpp::swagger::CustomController::createShared(docEndpoints);
    swaggerController->addEndpointsToRouter(router);

    /* Get connection handler component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);

    /* Get connection provider component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);

    /* Create server which takes provided TCP connections and passes them to HTTP connection handler */
    oatpp::network::Server server(connectionProvider, connectionHandler);

    std::function<bool()> condition = [](){
        return !lh::is_closing.load();
    };

    server.run(condition);

    OATPP_LOGI("Application::run", "Stopping Web Server");

    /* First, stop the ServerConnectionProvider so we don't accept any new connections */
    connectionProvider->stop();

    /* Signal the stop condition */
    lh::set_close(true);

    /* Finally, stop the ConnectionHandler and wait until all running connections are closed */
    connectionHandler->stop();

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void Application::close() {
    m_session.close();
}

}