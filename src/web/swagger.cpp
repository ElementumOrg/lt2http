#include "web/swagger.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createSwaggerController(const oatpp::web::server::api::Endpoints &endpoints) {
    return oatpp::swagger::CustomController::createShared(endpoints);
}
