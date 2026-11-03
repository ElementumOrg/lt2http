# Changelog

## Unreleased
- Added a shared library build (`lt2http.so` / `lt2http.dll`), started with `startWithLogAndArgs()`.
- Added linux-armv7_softfp and darwin-arm64 builds, linux-armv6 is built with a newer toolchain.
- Updated dependencies: Boost 1.86, OpenSSL 1.1.1w, oatpp 1.3.1, libtorrent 1.2.20, cpp-httplib 0.14.3, json_struct 1.0.2.
- Requests from localhost are always allowed without authentication.
- Fixed a missing config file being ignored without `--update`.
- `/changelog` returns plain text.

## 0.0.16
- Try to switch to original libtorrent repository.
- Changes to priority settings for torrent pieces.

## 0.0.13-0.0.15
- Added buffering information to endpoint /torrents/{infoHash}/files/{index}/status.
- More changes to priority setters.
- Fixed issues with Seek actions on 32-bit platforms.
- Fixed windows-x86 build to become static and do not require dll's.

## 0.0.9-0.0.12
- Added --update/-u cli argument support to update configuration, defined with --config, with current configuration.
- Added more default trackers.
- Added creation of directories for torrents_path and downloads_path.
- Added sorting to trackers list in /info.
- Added more information to log file.
- Fixed issue with calculating memory size on 32-bit systems.

## 0.0.7-0.0.8
- Added additional trackers modification for added torrents.
- Added support for setting libtorrent profile (not recommended, it can drop settings, customized by user).
- Added follow redirect and compression to http_client.

## 0.0.6
- Fixed torrents autoload on startup.
- Fixed issue with trying to download non-existing pieces.

## 0.0.5
- Initial changelog creation.
- Fixed issue with setting big amount of memory for memory storage.
- Fixed issue with crashing app while auto loading torrents on startup.
- Changed way to monitor tracker peers.
- For streaming endpoints added file priority setter to ensure download for non-memory storage.

## 0.0.1-0.0.4
- Initial implementation of lt2http.
