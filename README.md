lt2http 
======

What it is
----------

lt2http is a libtorrent-to-http application.

It is a web-server, responding with json, which uses libtorrent torrent library inside, to download and manage torrents, and can stream downloaded files via http and download files to memory (without using hard disks).

It has built-in Swagger.

By default, runs on port `65225`, so application can be accessed on `http://localhost:65225`.

Swagger is accessible on `http://localhost:65225/swagger/ui`

Checkout
----------
You can clone this repository with downloading all the submodules, in one command:
```
$ git clone --recursive https://github.com/ElementumOrg/lt2http.git
```


Submodules
----------
`lt2http` uses several external projects (header-only), so you need to download them first, if needed (if you did not clone repository with a `--recursive` key)

```
$ git submodule update --init --recursive --remote --no-fetch --depth=1
```

Dependencies
----------
`lt2http` has these dependencies:
- Boost (at least version 1.66.0)
- OpenSSL (at least version 1.1.1)
- Libtorrent (at least version 1.2.1)
- Oatpp (at least version 1.2.5)
- Oatpp-swagger (at least version 1.2.5)
- MiMalloc (at least version 2.0.0)
- Libbacktrace (latest master)

Build dependencies
----------
You can look at build commands at `scripts/build-local.sh` script.

By default everything is built with `Release` profile, meaning all the default optimizations and lack of debug symbols.

Default installation directory: `./local-env/`.

To use custom directory run:
```
sudo CROSS_ROOT=/usr/ CROSS_TRIPLE=x86_64-linux-gnu ./scripts/build-local.sh
```

Some notes regarding dependencies and `build-local.sh`:
- We force use of gcc-6, because cross-build docker image has gcc-6 (for non-Android targets). That is because building on a newer OS will require newer libc, so users with old Kernels will not be able to run it. You can use any compiler.
- All dependencies are compiled as static libraries to avoid having library dependencies in a `lt2http` binary.
- We force use of `C++17` to have same standard for all dependencies.
- `Oatpp` is compiled always in a `Release` profile, because we usually do not need to debug it.
- Boost installation makes a file `~/user-config.jam`, so if you don't want it to be overwritten - comment that in `build.sh`.
- MiMalloc is not used by default.

Build lt2http
----------
`lt2http` notes:
- Libtorrent RC1_2 is used.
- `C++17` is required from the compiler.
- `Cmake` requires minimum version of `3.15.0`.
- If you use profile `RelWithDebInfo` - Cmake will enable sanitizers for the binary: `-fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=bounds-strict`
- If you use profile `Debug` or `RelWithDebInfo` Cmake will add debug symbols in the binary, useful to find cause of a segmentation fault or some error.

Build locally process:
```
$ mkdir -p build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
```

Build using Docker:
There is a simple way, pull docker image and compile lt12http there.
```
$ make pull PLATFORM=linux-x64 # Pull Docker image for platform linux-x64 from Docker Hub
$ make linux-x64 # Run in linux-x64 Docker image
```

Build process helper:
----------

For easy of building there is `test_build.sh` with 3 options:
```
test_build.sh local # Run compilation on a local system, without Docker
test_build.sh sanitize # Run compilation with CMake build type RelWithDebInfo, which includes sanitizers.
test_build.sh docker # Run compilation in a Docker.
```

Prepare new target OS:
----------
- To build for specific OS, you need to create Dockerfile for that system.
- Add it into Makefile's list of platforms.
- Run Docker build with `PLATFORM=linux-x64 make env`, replace `linux-x64` with name of your Dockerfile.

