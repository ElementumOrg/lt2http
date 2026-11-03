#include "web/session.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createSessionController() { return SessionController::createShared(); }
