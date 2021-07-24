#pragma once

#include <string>

// This header contains application changelog.

std::string changes = R""""(
<b>Changes:</b>

<b>0.0.9-0.0.10:</b>
    - Added --update/-u cli argument support to update configuration, defined with --config, with current configuration.
    - Added more default trackers.
    - Added creation of directories for torrents_path and downloads_path.
    - Added sorting to trackers list in /info.

<b>0.0.7-0.0.8:</b>
    - Added additional trackers modification for added torrents.
    - Added support for setting libtorrent profile (not recommended, it can drop settings, customized by user).
    - Added follow redirect and compression to http_client.

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