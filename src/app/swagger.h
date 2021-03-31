#pragma once 

#include "oatpp-swagger/Model.hpp"
#include "oatpp-swagger/Resources.hpp"
#include "oatpp/core/macro/component.hpp"

#include <app/config.h>

namespace lh {

/**
 *  Swagger ui is served at
 *  http://host:port/swagger/ui
 */
class SwaggerComponent {
public:
  
  /**
   *  General API docs info
   */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::swagger::DocumentInfo>, swaggerDocumentInfo)([] {    
        oatpp::swagger::DocumentInfo::Builder builder;
        
        std::string uri = "127.0.0.1";
        if (web_interface != "0.0.0.0") {
            uri = web_interface;
        }
        uri = "http://" + uri + ":" + std::to_string(web_port);

        builder
            .setTitle("lt2http")
            .setDescription("Libtorrent to HTTP application")
            .setVersion(oatpp::String(lh::VERSION.c_str()))
            .setContactName("elgatito")
            .setContactUrl("https://elementum.surge.sh/")

            .setLicenseName("")
            .setLicenseUrl("")

            .addServer(uri.c_str(), "server on localhost");
        
        return builder.build();
  }());
  
  
    /**
    *  Swagger-Ui Resources (<oatpp-examples>/lib/oatpp-swagger/res)
    */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::swagger::Resources>, swaggerResources)([] {
        // Make sure to specify correct full path to oatpp-swagger/res folder !!!
        return oatpp::swagger::Resources::loadResources(OATPP_SWAGGER_RES_PATH);
    }());
  
};

}