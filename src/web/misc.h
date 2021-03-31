#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <app/config.h>
#include <app/application.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

class MiscController : public oatpp::web::server::api::ApiController {
  public:
    explicit MiscController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<MiscController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<MiscController>(objectMapper);
    }

    ENDPOINT("GET", "/", index) {
        const char *html = "<html lang='en'>"
                           "  <head>"
                           "    <meta charset=utf-8/>"
                           "  </head>"
                           "  <body>"
                           "    <a href='swagger/ui'>Checkout Swagger-UI page</a>"
                           "  </body>"
                           "</html>";
        auto response = createResponse(Status::CODE_200, html);
        response->putHeader(Header::CONTENT_TYPE, "text/html");
        return response;
    }

    ENDPOINT("GET", "/shutdown", shutdown) {
        OATPP_LOGE("MiscController::shutdown", "Stopping application after /shutdown request")
        lh::set_close(true);

        return createResponse(Status::CODE_200, "true");
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
