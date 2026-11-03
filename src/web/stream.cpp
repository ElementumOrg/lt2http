#include "web/stream.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createStreamController() { return StreamController::createShared(); }
