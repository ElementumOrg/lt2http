lt2http
=======

What it is
----------
lt2http is a libtorrent-to-HTTP application: a web server with a JSON API that uses [libtorrent](https://github.com/arvidn/libtorrent)
to download and manage torrents, streams downloaded files over HTTP and can download to memory, without using the disk.

It runs on port `65225` by default (`http://localhost:65225`), with Swagger documentation of the API at `http://localhost:65225/swagger/ui`.
In Kodi it is started by the [service.lt2http](https://github.com/ElementumOrg/service.lt2http) add-on, as a binary or as a shared library.

Running
-------
```
$ lt2http --config=lt2http.json     # settings from a JSON file
$ lt2http --web_port=65226          # every setting is also a command line option
$ lt2http --help                    # all options
$ lt2http --write=lt2http.json      # writes the current settings to a file and exits
```
`--config` with `--update` creates or updates the file with all settings. Torrent and resume files are kept in `torrents_path` (default: the current folder).
Requests from `127.0.0.1` never need authentication, others do when `web_login` or `web_password` is set.

Web UI: `http://<host>:65225/web/` (`/` opens it). It is built into the binary, so it works without any other files: add torrents
(magnet, link to a .torrent file, with request headers as `https://host/file.torrent|Cookie=a=b|User-Agent=Kodi`, or upload),
choose the storage and the files to download, see torrents with their files, trackers and pieces, copy file links to play or
download, remove torrents with or without their data, and see the settings (without passwords), libtorrent settings and state.
It is translated into the languages of Elementum, `web/public/i18n/` has the texts.

The Web UI is TypeScript with React and Mantine in `web/src/`. It is built into `web/dist/`, which is in git and built into
the binary, so building lt2http does not need Node.js. After changing `web/src/` or a translation, run `make web` (needs
Node.js) and commit `web/dist/`. `cd web && npm run dev` serves it with live reload, using an lt2http on port 65225.

Main API endpoints (see Swagger for all of them and their parameters):
- `/status`, `/info`, `/pause`, `/resume` - session state and control
- `/service/add/uri`, `/service/add/file` - add a torrent by magnet, URL, local path or uploaded file
- `/torrents`, `/torrents/{infoHash}/...` - list, inspect, pause, resume and remove torrents and their files
- `/torrents/{infoHash}/files/{index}/stream/{file_name}` - stream a file, with range requests
- `/torrents/{infoHash}/files/{index}/buffer` - download the start (or any part) and the end of a file first
- `/torrents/{infoHash}/files/download`, `/files/stop`, `/files/{index}/set_priority` - choose what to download
- `/torrents/{infoHash}/meta_info` - free JSON kept with the torrent (GET to read, POST to set, `null` removes it)
- `/torrents/{infoHash}/players/add`, `/players/remove` - register players, the torrent state is "Playing" while it has any. Players are removed after `player_timeout` seconds (default 60) without stream connections, like when Kodi crashes
- `/torrents/{infoHash}/trackers`, `/torrents/{infoHash}/pieces` - trackers and pieces state
- `/settings/get`, `/settings/set` - read and change settings at runtime
- `/settings/state` - settings without passwords, libtorrent settings, session state and statistics
- `/languages` - translations of the Web UI, `/info` and of `state_localized` (the `lang` parameter of `/info` and the torrent
  endpoints, or the `language` setting, picks one, like `de` or `pt-br`; `state_string` stays the same in every language)
- `/shutdown` - stop gracefully (exit code 0)

Tests
----------
- `make test` - unit tests, then `tests/smoke_test.sh`: an end-to-end test of the binary that needs no network
- `make test-public` - `tests/public_test.sh`: an end-to-end test with Big Buck Bunny, a public and legal torrent, downloaded from the internet (about 10 MB, its web seed works without peers). It adds the torrent, selects, downloads, streams and stops files, sets and edits meta info, buffers, restarts, removes the torrent with its data and streams from memory storage. Takes the binary with `LT2HTTP=<path>`, the local build by default
- Both run anywhere with curl and python3, without Kodi. `KEEP_WORK=1` keeps the logs

Checkout
----------
You can clone this repository with downloading all the submodules, in one command:
```
$ git clone --recursive https://github.com/ElementumOrg/lt2http.git
```


Submodules
----------
`lt2http` uses two header-only projects as submodules (cpp-httplib, json_struct), so you need to download them first, if you did not clone repository with a `--recursive` key:

```
$ git submodule update --init --recursive
```

Dependencies
----------
Versions are pinned in `scripts/versions.env`:
- Boost 1.86 (libtorrent 1.2 needs the asio API removed in Boost 1.87)
- OpenSSL 1.1.1
- Libtorrent 1.2 (lt2http uses its storage interface)
- Oatpp and Oatpp-swagger 1.3
- Libbacktrace (optional, for stack traces)

`scripts/install_dependencies.sh` builds all of them as static libraries inside the Docker images. For a native build, copy them from an image:
```
$ make local-env   # copies dependencies from the elementumorg/lt2http:linux-x64 image into ./local-env
$ make local       # builds build/cmake/local/lt2http with the host compiler
```
`LOCAL_PLATFORM=<platform>` picks another image; the host compiler has to match its toolchain (for example MinGW gcc for `windows-x64`).
On hosts without a matching image, build the dependencies natively with `PREFIX=$PWD/local-env scripts/install_dependencies.sh`.

Build lt2http
----------
`lt2http` is built with CMake (C++17). The `lt2http` target is the binary, `lt2http_library` the `lt2http.so` / `lt2http.dll`
shared library, both link the same objects. Options (`-D<option>=ON|OFF`):
- `static_runtime` - link the C++ runtime statically (default: ON)
- `enable_sanitizers` - build with address, leak and undefined behavior sanitizers (default: OFF)
- `build_tests` - build the unit tests, run with `ctest` (default: OFF)

Shared library
----------
The shared library exports a single function, used by the Kodi add-on through ctypes:
```c
int startWithLogAndArgs(const char *log_path, const char *args); // args: --config="/path/to/lt2http.json"
```
- It blocks until lt2http stops (call it from a thread) and returns 0 after `/shutdown`, 1 on failure (for example a missing config file).
- `args` is a command line, split like the platform shell does, so quote paths with spaces. Log lines are appended to `log_path`.
- It can be called again in the same process after it returned: the library cannot be unloaded, so it is reused on restarts.
  Only one instance can run at a time.
- It installs no signal handlers and never exits the process. All other symbols are hidden, so it can be loaded next to Elementum.
- On Windows file names are narrow (ANSI) strings: the add-on passes short (8.3) paths to support non-ASCII user folders.

Build using Docker
----------
Every platform has a Docker image with all dependencies, `elementumorg/lt2http:<platform>`, built from `docker/<platform>.Dockerfile`.
Platforms are the same as for Elementum: `android-arm`, `android-arm64`, `android-x64`, `android-x86`, `linux-armv6`, `linux-armv7`,
`linux-armv7_softfp`, `linux-arm64`, `linux-x64`, `linux-x86`, `windows-x64`, `windows-x86`, `darwin-x64`, `darwin-arm64`.
```
$ make pull PLATFORM=linux-x64   # or build the image locally: make env PLATFORM=linux-x64
$ make linux-x64                 # binary into build/linux-x64/
$ make linux-x64-shared          # shared library into build/linux-x64/
$ make all                       # everything, JOBS=4 platforms in parallel
```
`linux-armv6` uses a Bootlin toolchain (GCC 9.3, glibc 2.31) instead of the cross-compiler image one, which is too old for C++17.

See `make help` for all targets.

Build process helper:
----------

`test_build.sh <build> [linux|wsl]` builds lt2http, installs it into the service.lt2http of a local Kodi
(`linux`: `~/.kodi`, `wsl`: the Windows Kodi of `$WSL_USER`) with the add-on's version stamp, and restarts it there:
```
test_build.sh local # Native build with the 'make local-env' dependencies, without Docker
test_build.sh sanitize # Native build with sanitizers and debug information
test_build.sh docker # The binary, built in Docker
test_build.sh docker-library # The shared library, built in Docker (Kodi loads a new library on its next start)
```

Build troubleshooting
----------
- A compiler killed by the system (`Killed signal terminated program cc1plus`) is out of memory: lower `JOBS` or `BUILD_JOBS`.
- CMake keeps the toolchain flags of the first configure: after changing an image, remove `build/cmake/<platform>` (or `make clean`).
- Dependency limits: libtorrent 1.2 needs Boost < 1.87, and cpp-httplib after v0.14.3 needs OpenSSL 3 (we use 1.1.1, like libtorrent-go).
- macOS images build Boost with the b2 `darwin` toolset: the `clang` toolsets add `-m64`, which osxcross turns into x86_64.
- 32-bit soft-float ARM (`linux-armv7_softfp`) is built for ARMv5TE + VFP, matching its libstdc++.

Prepare new target OS:
----------
- Create `docker/<platform>.Dockerfile`: it sets the toolchain environment for `cmake/toolchains/cross.cmake` and `scripts/install_dependencies.sh`.
- Add it into Makefile's list of platforms.
- Build the image with `make env PLATFORM=<platform>`.

Release
-------

Release of a binary part of lt2http means compiling binaries for all platforms and putting them into <https://github.com/ElementumOrg/lt2http-binaries> repository (if we run on the git tag).

Release is done with `release.sh` script.

Changes go into `changelog.md` (rename `Unreleased` to the version when tagging). It is embedded into lt2http at build time and served by `/changelog`.