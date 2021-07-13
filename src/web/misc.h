#pragma once

#include <boost/algorithm/string/replace.hpp>

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <app/changelog.h>
#include <app/config.h>
#include <app/application.h>

#include "web/basic_auth.h"

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

    ENDPOINT_INFO(index) {
        info->summary = "Index";

        info->addResponse<String>(Status::CODE_200, "text/html");
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

    ENDPOINT_INFO(changelog) {
        info->summary = "Changelog";

        info->addResponse<String>(Status::CODE_200, "text/html");
    }
    ENDPOINT("GET", "/changelog", changelog) {
        boost::replace_all(changes, "\n", "<br>");

        std::string html = "<html lang='en'>"
                           "  <head>"
                           "    <meta charset=utf-8/>"
                           "  </head>"
                           "  <body>"
                           +  changes +
                           "  </body>"
                           "</html>";

        auto response = createResponse(Status::CODE_200, html.c_str());
        response->putHeader(Header::CONTENT_TYPE, "text/html");
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

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
