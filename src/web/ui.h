#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <app/application.h>
#include <app/i18n.h>

#include <utils/strings.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

// The Web UI, files of web/ built into the binary
class UiController : public oatpp::web::server::api::ApiController {
  private:
    std::shared_ptr<OutgoingResponse> redirect(const char *location) {
        auto response = createResponse(Status::CODE_302, "");
        response->putHeader("Location", location);
        return response;
    }

    static const char *content_type(const std::string &path) {
        auto ext = get_file_ext(path);
        if (ext == "html")
            return "text/html; charset=utf-8";
        if (ext == "js")
            return "text/javascript; charset=utf-8";
        if (ext == "css")
            return "text/css; charset=utf-8";
        if (ext == "json")
            return "application/json; charset=utf-8";
        if (ext == "svg")
            return "image/svg+xml";
        return "text/plain; charset=utf-8";
    }

  public:
    explicit UiController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<UiController> createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper)) {
        return std::make_shared<UiController>(objectMapper);
    }

    ENDPOINT_INFO(index) { info->summary = "Redirects to the Web UI"; }
    ENDPOINT("GET", "/", index) { return redirect("/web/"); }

    ENDPOINT_INFO(web) { info->summary = "Web UI"; }
    ENDPOINT("GET", "/web/*", web, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        std::string path = request->getPathTail() ? *request->getPathTail() : "";
        path = path.substr(0, path.find_first_of("?#"));
        if (path.empty())
            path = "index.html";

        auto file = lh::find_web_file(path);
        if (!file)
            return createResponse(Status::CODE_404, "Not found");

        auto response = createResponse(Status::CODE_200, oatpp::String(file->content.data(), file->content.size()));
        response->putHeader(Header::CONTENT_TYPE, content_type(path));
        // Files change with a new lt2http version
        response->putHeader("Cache-Control", "no-cache");
        return response;
    }

    ENDPOINT_INFO(languages) {
        info->summary = "Languages of the Web UI, /info and state_localized, and the language setting";

        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/languages", languages) {
        using Fields = oatpp::Fields<oatpp::Any>;
        auto list = oatpp::List<oatpp::Any>::createShared();
        for (const auto &language : lh::languages()) {
            auto item = Fields::createShared();
            item->push_back({"code", oatpp::Any(oatpp::String(language.code))});
            item->push_back({"name", oatpp::Any(oatpp::String(language.name))});
            list->push_back(oatpp::Any(item));
        }

        auto result = Fields::createShared();
        result->push_back({"default", oatpp::Any(oatpp::String(lh::resolve_language(lh::config().language)))});
        result->push_back({"languages", oatpp::Any(list)});
        return createDtoResponse(Status::CODE_200, result);
    }
};

#include OATPP_CODEGEN_END(ApiController)
