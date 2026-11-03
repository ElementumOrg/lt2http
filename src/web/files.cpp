#include "web/files.h"
#include "web/controllers.h"

lh::ControllerPtr lh::createFilesController() { return FilesController::createShared(); }
