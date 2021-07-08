#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <json_struct.h>

#include <app/application.h>

#include "web/basic_auth.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

class SettingsController : public oatpp::web::server::api::ApiController {
  public:
    std::shared_ptr<AuthorizationHandler> m_authHandler = std::make_shared<lh::CustomBasicAuthorizationHandler>();

    explicit SettingsController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<SettingsController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<SettingsController>(objectMapper);
    }

    ENDPOINT_INFO(get) {
        info->summary = "Get global settings";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/settings/get", get,
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto response = createResponse(Status::CODE_200, oatpp::String(JS::serializeStruct(lh::config()).c_str()));
        response->putHeader(Header::CONTENT_TYPE, "application/json");
        return response;
    }

    ENDPOINT_INFO(set) {
        info->summary = "Set global settings";

        info->addSecurityRequirement("basic_auth");

        info->addConsumes<String>("application/json");
        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("POST", "/settings/set", set, 
        BODY_STRING(String, body),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        lh::Config &config = lh::config();
        JS::ParseContext context(body->c_str());
        auto err = context.parseTo(config);

        OATPP_ASSERT_HTTP(err == JS::Error::NoError, Status::CODE_500,
                          ("Error converting json: " + context.makeErrorString()).c_str())

        OATPP_LOGW("SettingsController::set", "Configuration updated to: %s", JS::serializeStruct(config).c_str())
        lh::session().reconfigure();
        
        auto response = createResponse(Status::CODE_200, oatpp::String(JS::serializeStruct(config).c_str()));
        response->putHeader(Header::CONTENT_TYPE, "application/json");
        return response;
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
