#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <json_struct.h>

#include <app/application.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

class SettingsController : public oatpp::web::server::api::ApiController {
  public:
    explicit SettingsController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<SettingsController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<SettingsController>(objectMapper);
    }

    ENDPOINT("GET", "/settings/get", get) {
        auto response = createResponse(Status::CODE_200, oatpp::String(JS::serializeStruct(lh::config()).c_str()));
        response->putHeader(Header::CONTENT_TYPE, "application/json");
        return response;
    }

    ENDPOINT("POST", "/settings/set", set, BODY_STRING(String, body)) {

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
