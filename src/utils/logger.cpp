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

    m_out << "[";

    const char *timeFormat = "%Y-%m-%d %H:%M:%S";
    time_t seconds = std::chrono::duration_cast<std::chrono::seconds>(time).count();
    struct tm now {};
    localtime_r(&seconds, &now);
#ifdef OATPP_DISABLE_STD_PUT_TIME
    char timeBuffer[50];
    strftime(timeBuffer, sizeof(timeBuffer), m_config.timeFormat, &now);
    m_out << timeBuffer;
#else
    m_out << std::put_time(&now, timeFormat);
#endif

    m_out << "]";

    switch (priority) {
    case PRIORITY_V:
        m_out << "\033[0;0m VERB \033[0m";
        break;

    case PRIORITY_D:
        m_out << "\033[34;0m DEBU \033[0m";
        break;

    case PRIORITY_I:
        m_out << "\033[32;0m INFO \033[0m";
        break;

    case PRIORITY_W:
        m_out << "\033[45;0m WARN \033[0m";
        break;

    case PRIORITY_E:
        m_out << "\033[41;0m ERRO \033[0m";
        break;

    default:
        m_out << " " << priority << " ";
    }

    m_out << "|";

    if (message.empty()) {
        m_out << " > " << tag << std::endl;
    } else {
        m_out << " " << tag << " > " << message << std::endl;
    }
}

}