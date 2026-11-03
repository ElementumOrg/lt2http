#pragma once

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/web/server/handler/AuthorizationHandler.hpp"
#include "oatpp/web/server/interceptor/RequestInterceptor.hpp"
#include "oatpp/web/protocol/http/outgoing/ResponseFactory.hpp"

namespace lh {

class AuthInterceptor : public oatpp::web::server::interceptor::RequestInterceptor {
  private:
    using OutgoingResponse = oatpp::web::protocol::http::outgoing::Response;
    using Status = oatpp::web::protocol::http::Status;
    using ResponseFactory = oatpp::web::protocol::http::outgoing::ResponseFactory;

    oatpp::web::server::handler::BasicAuthorizationHandler m_authHandler;

  public:
    AuthInterceptor();

    std::shared_ptr<OutgoingResponse> intercept(const std::shared_ptr<IncomingRequest>& request) override;
};

} // namespace lh