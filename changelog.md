# Changelog

## 0.0.17 (2026-11-03)
- Added lt2http as a shared library (`lt2http.so` / `lt2http.dll`), started with `startWithLogAndArgs()`.
- Added linux-armv7_softfp and darwin-arm64 (Apple Silicon) builds.
- Requests from localhost are always allowed without authentication.
- Torrent information and status come from a state kept up to date in the background, so requests are faster and `/info` no longer slows down on torrents with many pieces.
- The name of a magnet link is shown before its metadata is downloaded.
- Fixed torrents loaded at startup waiting for metadata again ("Downloading metadata") after a restart.
- Fixed a torrent with an unreadable resume file not being loaded at all.
- Streaming answers a Range request with exactly the requested bytes instead of the rest of the file, and with 416 for a range outside the file.
- Fixed streaming that could send not yet downloaded data when a read covered more than one piece.
- Fixed `/torrents/{infoHash}/generate` returning a truncated torrent file.
- Fixed wrong `buffering_total` values in `/torrents/{infoHash}/files/{index}/status`.
- Fixed possible crashes when torrents are added, removed or streamed at the same time (like removing a torrent that is still streaming), and when lt2http is stopped or started again.
- Fixed a missing config file being silently ignored: lt2http now stops with an error unless `--update` is given.
- Adding a torrent reports an error when the torrent file cannot be saved.
- Shutdown no longer waits up to 5 minutes when the trackers list cannot be downloaded.
- `/changelog` returns plain text.
- Add-on: starts lt2http as a shared library when the binary cannot be started (Android 10+), or always with the "Force start as a library" setting.
- Add-on: detects darwin-arm64 and lists linux-armv7_softfp in the platform setting.
- Add-on: the port setting is used by lt2http (it was ignored), changing it restarts lt2http. Remote host and port settings are removed.
- Add-on: an lt2http left running by a crashed Kodi is stopped before a new one starts.
- Add-on: more reliable binary updates: the newest release is picked (v0.0.16 was taken as older than v0.0.9), and updates work when the Kodi data folder is not writable.
- Add-on: removes stale entries that piled up in the add-on settings.
- Add-on: translations are loaded on Kodi 19 (Matrix) and later.
- Add-on: updates come from the ElementumOrg repository (`repository.elementumorg`).

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
