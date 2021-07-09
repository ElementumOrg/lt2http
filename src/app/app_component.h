#pragma once

#include <iostream>

#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/web/server/interceptor/AllowCorsGlobal.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"

// #include "oatpp/network/virtual_/client/ConnectionProvider.hpp"
#include "oatpp/network/virtual_/server/ConnectionProvider.hpp"
#include "oatpp/network/virtual_/Interface.hpp"

#include "oatpp/network/ConnectionPool.hpp"

#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "oatpp/core/macro/component.hpp"

#include <app/application.h>
#include <app/config.h>
#include <app/swagger.h>

#include <web/basic_auth.h>
#include <web/error_handler.h>
#include <web/request_logger.h>

namespace lh {

/**
 *  Class which creates and holds Application components and registers
 * components in oatpp::base::Environment Order of components initialization is
 * from top to bottom
 */
class AppComponent {
  public:
    /**
     *  Swagger component
     */
    SwaggerComponent swaggerComponent;

    /**
     * Create ObjectMapper component to serialize/deserialize DTOs in
     * Contoller's API
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper)([] {
        auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
        objectMapper->getDeserializer()->getConfig()->allowUnknownFields = true;
        return objectMapper;
    }());

    /**
     *  Create ConnectionProvider component which listens on the port
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, serverConnectionProvider)([] {
        return oatpp::network::tcp::server::ConnectionProvider::createShared(
            {web_interface.c_str(), static_cast<v_uint16>(web_port), oatpp::network::Address::IP_4});
    }());

    /**
     *  Create Router component
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, httpRouter)([] { 
        return oatpp::web::server::HttpRouter::createShared(); 
    }());

    /**
     *  Create ConnectionHandler component which uses Router component to route
     * requests
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, serverConnectionHandler)([] {
        OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
        OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);

        // Increase streaming buffer to 16kb, to make less requests to the storage
        auto config = std::make_shared<oatpp::web::server::HttpProcessor::Config>();
        config->headersOutBufferInitial = 16 * 1024;

        auto connectionHandler = std::make_shared<oatpp::web::server::HttpConnectionHandler>(router, config);
        
        // Set default error handler
        connectionHandler->setErrorHandler(std::make_shared<lh::ErrorHandler>(objectMapper));

        // Add request tracker
        connectionHandler->addResponseInterceptor(std::make_shared<lh::ResponseLogger>());

        // Enable CORS globally
        connectionHandler->addRequestInterceptor(std::make_shared<oatpp::web::server::interceptor::AllowOptionsGlobal>());
        connectionHandler->addResponseInterceptor(std::make_shared<oatpp::web::server::interceptor::AllowCorsGlobal>());

        // Add authentication handler
        connectionHandler->addRequestInterceptor(std::make_shared<lh::AuthInterceptor>());

        return connectionHandler;
    }());
};

}