#include "web/ui.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createUiController() { return UiController::createShared(); }
