#pragma once

#include "oatpp-swagger/Resources.hpp"
#include "oatpp-swagger/Generator.hpp"

#include "oatpp/web/server/api/ApiController.hpp"

#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "oatpp/web/protocol/http/outgoing/StreamingBody.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

namespace oatpp { namespace swagger {

/**
 * Controller for Swagger-UI. Extends &id:oatpp::web::server::api::ApiController;. <br>
 * Should be used with oatpp Simple API and multithreaded server. <br>
 * For more information about integrating swagger-ui in oatpp application see [oatpp-swagger module](/docs/modules/oatpp-swagger/). <br>
 * Exposed endpoints:
 * <ul>
 *   <li>&id:ENDPOINT;("GET", "/api-docs/oas-3.0.0.json", api) - Server Open API Specification.</li>
 *   <li>&id:ENDPOINT;("GET", "/swagger/ui", getUIRoot) - Server Swagger-UI. (index.html).</li>
 *   <li>&id:ENDPOINT;("GET", "/swagger/{filename}", getUIResource) - Serve Swagger-UI resources.</li>
 * </ul>
 */
class CustomController : public oatpp::web::server::api::ApiController {
private:
  oatpp::Object<oas3::Document> m_document;
  std::shared_ptr<oatpp::swagger::Resources> m_resources;
public:
  CustomController(const std::shared_ptr<ObjectMapper>& objectMapper,
             const oatpp::Object<oas3::Document>& document,
             const std::shared_ptr<oatpp::swagger::Resources>& resources)
    : oatpp::web::server::api::ApiController(objectMapper)
    , m_document(document)
    , m_resources(resources)
  {}
public:

  /**
   * Create shared Controller.
   * @param endpointsList - &id:oatpp::web::server::api::ApiController::Endpoints;
   * @param documentInfo - &id:oatpp::swagger::DocumentInfo;.
   * @param resources - &id:oatpp::swagger::Resources;.
   * @return - Controller.
   */
  static std::shared_ptr<CustomController> createShared(const std::shared_ptr<Endpoints>& endpointsList,
                                                  OATPP_COMPONENT(std::shared_ptr<oatpp::swagger::DocumentInfo>, documentInfo),
                                                  OATPP_COMPONENT(std::shared_ptr<oatpp::swagger::Resources>, resources)){
    
    auto serializerConfig = oatpp::parser::json::mapping::Serializer::Config::createShared();
    serializerConfig->includeNullFields = false;
    
    auto deserializerConfig = oatpp::parser::json::mapping::Deserializer::Config::createShared();
    deserializerConfig->allowUnknownFields = false;
    
    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared(serializerConfig, deserializerConfig);

    std::shared_ptr<Generator::Config> generatorConfig;
    try {
      generatorConfig = OATPP_GET_COMPONENT(std::shared_ptr<Generator::Config>);
    } catch (std::runtime_error e) {
      generatorConfig = std::make_shared<Generator::Config>();
    }

    Generator generator(generatorConfig);
    auto document = generator.generateDocument(documentInfo, endpointsList);
    
    return std::make_shared<CustomController>(objectMapper, document, resources);
  }
  
#include OATPP_CODEGEN_BEGIN(ApiController)
  
  ENDPOINT("GET", "/api-docs/oas-3.0.0.json", api) {
    return createDtoResponse(Status::CODE_200, m_document);
  }
  
  ENDPOINT("GET", "/swagger/ui", getUIRoot) {
    const char *html = R"(<!-- HTML for static distribution bundle build -->
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8">
    <title>Swagger UI</title>
    <link rel="stylesheet" type="text/css" href="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/3.46.0/swagger-ui.min.css" >
    <link rel="icon" type="image/png" href="./favicon-32x32.png" sizes="32x32" />
    <link rel="icon" type="image/png" href="./favicon-16x16.png" sizes="16x16" />
    <style>
      html
      {
        box-sizing: border-box;
        overflow: -moz-scrollbars-vertical;
        overflow-y: scroll;
      }

      *,
      *:before,
      *:after
      {
        box-sizing: inherit;
      }

      body
      {
        margin:0;
        background: #fafafa;
      }
    </style>
  </head>

  <body>
    <div id="swagger-ui"></div>

    <script src="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/3.46.0/swagger-ui-bundle.js" charset="UTF-8"> </script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/3.46.0/swagger-ui-standalone-preset.js" charset="UTF-8"> </script>
    <script>
    window.onload = function() {
      // Begin Swagger UI call region
      const ui = SwaggerUIBundle({
        url: "/api-docs/oas-3.0.0.json",
        dom_id: '#swagger-ui',
        deepLinking: true,
        presets: [
          SwaggerUIBundle.presets.apis,
          SwaggerUIStandalonePreset
        ],
        plugins: [
          SwaggerUIBundle.plugins.DownloadUrl
        ],
        layout: "StandaloneLayout"
      })
      // End Swagger UI call region

      window.ui = ui
    }
  </script>
  </body>
</html>)";
        auto response = createResponse(Status::CODE_200, html);
        response->putHeader(Header::CONTENT_TYPE, "text/html");
        return response;
  }
    
#include OATPP_CODEGEN_END(ApiController)
  
};
  
}}
