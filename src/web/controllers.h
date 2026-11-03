#pragma once

#include <memory>

#include "oatpp/web/server/api/ApiController.hpp"

// Each controller is compiled in its own source file: their endpoints are slow to compile,
// this way they build in parallel instead of all in application.cpp.
namespace lh {

using ControllerPtr = std::shared_ptr<oatpp::web::server::api::ApiController>;

ControllerPtr createSettingsController();
ControllerPtr createMiscController();
ControllerPtr createSessionController();
ControllerPtr createTorrentsController();
ControllerPtr createFilesController();
ControllerPtr createStreamController();
ControllerPtr createSwaggerController(const oatpp::web::server::api::Endpoints &endpoints);

} // namespace lh
