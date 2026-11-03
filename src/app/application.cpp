#include "application.h"

#include <iostream>
#include <memory>

#include <boost/stacktrace.hpp>
#include <libtorrent/version.hpp>

#include <app/config.h>

#include <oatpp/network/Server.hpp>

#include <app/app_component.h>

#include <web/controllers.h>

namespace lh {

Application::Application(int &argc, char *argv[]) : m_config(argc, argv), m_session(m_config) {}

Application::~Application() = default;

lh::Config &Application::config() { return m_config; }

lh::Session &Application::session() { return m_session; }

void Application::run() {
    lh::web_interface = m_config.web_interface;
    lh::web_port = m_config.web_port;

    if (!m_config.web_login.empty() || !m_config.web_password.empty()) {
        OATPP_LOGI("Application::run", "Initializing Web Server on http://%s:%d with authentication for %s:%s", web_interface.c_str(),
                   web_port, m_config.web_login.c_str(), m_config.web_password.c_str());
    } else {
        OATPP_LOGI("Application::run", "Initializing Web Server on http://%s:%d", web_interface.c_str(), web_port);
    }

    /* Register components in scope of run() */
    AppComponent components;

    auto router = components.httpRouter.getObject();
    oatpp::web::server::api::Endpoints docEndpoints;
    for (const auto &controller : {createSettingsController(), createMiscController(), createSessionController(),
                                   createTorrentsController(), createFilesController(), createStreamController()})
        docEndpoints.append(router->addController(controller)->getEndpoints());

    router->addController(createSwaggerController(docEndpoints));

    /* Get connection handler component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);

    /* Get connection provider component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);

    /* Create server which takes provided TCP connections and passes them to HTTP connection handler */
    oatpp::network::Server server(connectionProvider, connectionHandler);

    std::function<bool()> condition = []() { return !lh::is_closing.load(); };

    // Loads trackers and previous torrents in a session thread.
    m_session.run();

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

void Application::close() { m_session.close(); }

} // namespace lh

// Custom signal handler to print stacktrace and exit gracefully.
void signalHandler(int signum) {
    OATPP_LOGE("Application::signalHandler", "Got interrupt signal (%d). Stopping application gracefully", signum);
    lh::set_close(true);

    if (signum == SIGSEGV) {
        std::cerr << boost::stacktrace::stacktrace();
        std::exit(signum);
    }
}
