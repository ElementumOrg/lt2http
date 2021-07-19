#pragma once

#include <string>

// This header contains application changelog.

std::string changes = R""""(
<b>Changes:</b>

<b>0.0.6:</b>
    - Fixed torrents autoload on startup.
    - Fixed issue with trying to download non-existing pieces.

<b>0.0.5:</b>
    - Initial changelog creation.
    - Fixed issue with setting big amount of memory for memory storage.
    - Fixed issue with crashing app while auto loading torrents on startup.
    - Changed way to monitor tracker peers.
    - For streaming endpoints added file priority setter to ensure download for non-memory storage.

<b>0.0.1-0.0.4:</b>
    - Initial implementation of lt2http.

)"""";