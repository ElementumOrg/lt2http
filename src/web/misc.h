#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <app/application.h>
#include <app/build_info.h>
#include <app/config.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

class MiscController : public oatpp::web::server::api::ApiController {
  public:
    explicit MiscController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<MiscController>
    createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<MiscController>(objectMapper);
    }

    ENDPOINT_INFO(changelog) {
        info->summary = "Changelog";

        info->addResponse<String>(Status::CODE_200, "text/plain");
    }
    ENDPOINT("GET", "/changelog", changelog) {
        auto response = createResponse(Status::CODE_200, lt2http_changelog);
        response->putHeader(Header::CONTENT_TYPE, "text/plain; charset=utf-8");
        return response;
    }

    ENDPOINT_INFO(shutdown) {
        info->summary = "Shutdown application gracefully";

        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/shutdown", shutdown) {
        OATPP_LOGE("MiscController::shutdown", "Stopping application after /shutdown request")
        lh::set_close(true);

        return createResponse(Status::CODE_200, "true");
    }
};

#include OATPP_CODEGEN_END(ApiController)
