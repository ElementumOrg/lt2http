#pragma once

#include <app/application.h>

#include "oatpp/web/server/api/ApiController.hpp"

namespace lh {

class CustomAuthorizationObject : public oatpp::web::server::handler::AuthorizationObject {
public:

  CustomAuthorizationObject(const oatpp::String& pAuthString)
    : authString(pAuthString)
  {}

  oatpp::String authString;

};

class CustomBasicAuthorizationHandler : public oatpp::web::server::handler::BasicAuthorizationHandler {
public:

    CustomBasicAuthorizationHandler()
        : BasicAuthorizationHandler("lt2http")
    {}

    std::shared_ptr<AuthorizationObject> authorize(const oatpp::String& userId, const oatpp::String& password) override {
        auto config = lh::config();
        if (config.web_login.empty() && config.web_password.empty()) {
            return std::make_shared<CustomAuthorizationObject>(userId + ":" + password);
        } 
        if (userId->std_str() == config.web_login && password->std_str() == config.web_password) {
            return std::make_shared<CustomAuthorizationObject>(userId + ":" + password);
        }
        
        return nullptr;
  }

};

}