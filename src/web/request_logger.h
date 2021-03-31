#include "oatpp/web/server/interceptor/ResponseInterceptor.hpp"
#include <oatpp/web/server/interceptor/RequestInterceptor.hpp>

namespace lh {

class ResponseLogger : public oatpp::web::server::interceptor::ResponseInterceptor {
  public:
    std::shared_ptr<OutgoingResponse> intercept(const std::shared_ptr<IncomingRequest> &request,
                                                const std::shared_ptr<OutgoingResponse> &response) override {
        auto sl = request->getStartingLine();
        OATPP_LOGD("HTTP::Log", "[%d] %s http://%s%s", response->getStatus().code, sl.method.std_str().c_str(),
                   request->getHeader("Host") != nullptr ? request->getHeader("Host")->c_str() : "NULL", sl.path.std_str().c_str())
        return response;
    }
};

} // namespace lh