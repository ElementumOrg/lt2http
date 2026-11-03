#include "web/torrents.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createTorrentsController() { return TorrentsController::createShared(); }
