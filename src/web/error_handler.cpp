#include "error_handler.h"

namespace lh {

ErrorHandler::ErrorHandler(const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper)
  : m_objectMapper(objectMapper)
{}

std::shared_ptr<ErrorHandler::OutgoingResponse>
ErrorHandler::handleError(const Status& status, const oatpp::String& message, const Headers& headers) {
    OATPP_LOGE("ErrorHandler::handleError", "Sending error: %d, %s", status.code, message->c_str());
    for (auto &h : headers.getAll()) {
        OATPP_LOGE("ErrorHandler::handleError", "header %s=%s", h.first.std_str().c_str(), h.second.std_str().c_str());
    }

    auto error = StatusDto::createShared();
    error->status = "ERROR";
    error->code = status.code;
    error->message = message;

    auto response = ResponseFactory::createResponse(status, error, m_objectMapper);

    for(const auto& pair : headers.getAll()) {
        response->putHeader(pair.first.toString(), pair.second.toString());
    }

    return response;
}

}