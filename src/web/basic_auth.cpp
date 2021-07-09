#include "basic_auth.h"

#include <app/application.h>

namespace lh {

AuthInterceptor::AuthInterceptor() = default;

std::shared_ptr<AuthInterceptor::OutgoingResponse> AuthInterceptor::intercept(const std::shared_ptr<IncomingRequest>& request) {
    auto config = lh::config();
    if (!config.is_auth_needed())
        return nullptr;

    auto authHeader = request->getHeader(oatpp::web::protocol::http::Header::AUTHORIZATION);

    // Default return object is DefaultBearerAuthorizationObject
    auto authObject = std::static_pointer_cast<oatpp::web::server::handler::DefaultBasicAuthorizationObject>(
        m_authHandler.handleAuthorization(authHeader));

    if (authObject->userId->std_str() == config.web_login && authObject->password->std_str() == config.web_password) {
        // Authorized
        return nullptr;
    }

    throw oatpp::web::protocol::http::HttpError(oatpp::web::protocol::http::Status::CODE_401, "Unauthorized", {});
}

}