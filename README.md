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

Main API endpoints (see Swagger for all of them and their parameters):
- `/status`, `/info`, `/pause`, `/resume` - session state and control
- `/service/add/uri`, `/service/add/file` - add a torrent by magnet, URL, local path or uploaded file
- `/torrents`, `/torrents/{infoHash}/...` - list, inspect, pause, resume and remove torrents and their files
- `/torrents/{infoHash}/files/{index}/stream/{file_name}` - stream a file, with range requests
- `/settings/get`, `/settings/set` - read and change settings at runtime
- `/shutdown` - stop gracefully (exit code 0)

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

`scripts/install_dependencies.sh` builds all of them as static libraries. It is used by the Docker images, and for a native build:
```
$ make local-env   # installs dependencies into ./local-env
$ make local       # builds build/cmake/local/lt2http with the host compiler
```

Build lt2http
----------
`lt2http` is built with CMake (C++14). Options (`-D<option>=ON|OFF`):
- `build_library` - build the `lt2http.so` / `lt2http.dll` shared library instead of the binary (default: OFF)
- `static_runtime` - link the C++ runtime statically (default: ON)
- `enable_sanitizers` - build with address, leak and undefined behavior sanitizers (default: OFF)

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
`linux-armv6` uses a Bootlin toolchain (GCC 9.3, glibc 2.31) instead of the cross-compiler image one, which is too old for C++14.

See `make help` for all targets.

Build process helper:
----------

For easy of building there is `test_build.sh` with 3 options, it copies the result into the local Kodi installation:
```
test_build.sh local # Run compilation on a local system, without Docker
test_build.sh sanitize # Run compilation with sanitizers and debug information.
test_build.sh docker # Run compilation in a Docker.
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