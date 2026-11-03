#include "web/misc.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createMiscController() { return MiscController::createShared(); }
