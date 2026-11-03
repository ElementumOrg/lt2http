#include "application.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <utils/logger.h>

#include <oatpp/network/Server.hpp>

#include <boost/stacktrace.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options/parsers.hpp>

namespace {

// Runs lt2http until it is stopped, returns 0 on a requested shutdown and 1 on failure.
int run(int argc, char *argv[], std::ostream &out) {
    auto const logger = std::make_shared<lh::CustomLogger>(out);
    oatpp::base::Environment::init(logger);

    int code = 0;
    try {
        lh::Application::init(argc, argv);

        lh::app().run();
    } catch (const std::exception &e) {
        code = 1;
        OATPP_LOGE("main", "Exception: %s", e.what())
        OATPP_LOGE("main", "Exception information: %s", boost::diagnostic_information(e, true).c_str())
        out << boost::stacktrace::stacktrace();
    } catch (...) {
        code = 1;
        OATPP_LOGE("main", "Exception: %s", typeid(std::current_exception()).name())
        out << boost::stacktrace::stacktrace();
    }

    try {
        if (lh::Application::initialized())
            lh::app().close();
        lh::Application::destroy();
    } catch (const std::exception &e) {
        OATPP_LOGE("main", "Failed closing application with Exception: %s", e.what())
    } catch (...) {
        OATPP_LOGE("main", "Unknown Exception: %s", typeid(std::current_exception()).name())
    }

    oatpp::base::Environment::destroy();

    return code;
}

}  // namespace

#ifdef LT2HTTP_LIBRARY

#ifdef _WIN32
#define LH_EXPORT __declspec(dllexport)
#else
#define LH_EXPORT __attribute__((visibility("default")))
#endif

// Entry point for loading lt2http as a shared library (from the Kodi add-on, via ctypes).
// Blocks until lt2http stops, log lines are appended to log_path.
// args is a command line, e.g. --config="/path/to/lt2http.json", quoted like the platform shell does.
extern "C" LH_EXPORT int startWithLogAndArgs(const char *log_path, const char *args) {
    std::ofstream log(log_path, std::ios::app);
    std::ostream &out = log.is_open() ? log : std::cout;

#ifdef _WIN32
    auto arguments = boost::program_options::split_winmain(args ? args : "");
#else
    auto arguments = boost::program_options::split_unix(args ? args : "");
#endif
    arguments.insert(arguments.begin(), "lt2http");

    std::vector<char *> argv;
    for (auto &argument : arguments)
        argv.push_back(&argument[0]);
    argv.push_back(nullptr);

    // No signal handlers here, they belong to the host process.
    return run(static_cast<int>(arguments.size()), argv.data(), out);
}

#else

auto main(int argc, char *argv[]) -> int {
    // Redefine signal handlers for proper logging.
    std::signal(SIGINT, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGSEGV, signalHandler);

    return run(argc, argv, std::cout);
}

#endif
