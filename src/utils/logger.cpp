#include "logger.h"

namespace lh {

#if (defined(WIN32) || defined(_WIN32)) && defined(_WIN64)
struct tm *localtime_r(time_t *_clock, struct tm *_result) {
    _localtime64_s(_result, _clock);
    return _result;
}
#elif (defined(WIN32) || defined(_WIN32)) && not defined(_WIN64)
struct tm *localtime_r(time_t *_clock, struct tm *_result) {
    _localtime32_s(_result, _clock);
    return _result;
}
#endif

void CustomLogger::log(v_uint32 priority, const std::string &tag, const std::string &message) {
    auto time = std::chrono::system_clock::now().time_since_epoch();

    std::lock_guard<std::mutex> lock(m_lock);

    std::cout << "[";

    const char *timeFormat = "%Y-%m-%d %H:%M:%S";
    time_t seconds = std::chrono::duration_cast<std::chrono::seconds>(time).count();
    struct tm now {};
    localtime_r(&seconds, &now);
#ifdef OATPP_DISABLE_STD_PUT_TIME
    char timeBuffer[50];
    strftime(timeBuffer, sizeof(timeBuffer), m_config.timeFormat, &now);
    std::cout << timeBuffer;
#else
    std::cout << std::put_time(&now, timeFormat);
#endif

    std::cout << "]";

    switch (priority) {
    case PRIORITY_V:
        std::cout << "\033[0;0m VERB \033[0m";
        break;

    case PRIORITY_D:
        std::cout << "\033[34;0m DEBU \033[0m";
        break;

    case PRIORITY_I:
        std::cout << "\033[32;0m INFO \033[0m";
        break;

    case PRIORITY_W:
        std::cout << "\033[45;0m WARN \033[0m";
        break;

    case PRIORITY_E:
        std::cout << "\033[41;0m ERRO \033[0m";
        break;

    default:
        std::cout << " " << priority << " ";
    }

    std::cout << "|";

    if (message.empty()) {
        std::cout << " > " << tag << std::endl;
    } else {
        std::cout << " " << tag << " > " << message << std::endl;
    }
}

}