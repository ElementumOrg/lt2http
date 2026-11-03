#include "web/settings.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createSettingsController() { return SettingsController::createShared(); }
