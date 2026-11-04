# AGENTS.md

Notes for AI coding agents (and people) working on lt2http. README.md has the user documentation.

## What it is

lt2http is a C++17 HTTP server around libtorrent 1.2: it adds torrents, downloads them to disk or memory and streams their
files over HTTP. It runs as a binary or as a shared library (`startWithLogAndArgs()`) inside the Kodi add-on
`service.lt2http`, which is a separate repository. It is built for Android, Linux, Windows and macOS in Docker images.

## Layout

- `src/app/` - startup, config (`config.h`: every setting, JSON and command line), i18n (translations, embedded web files)
- `src/bittorrent/` - `Session` (libtorrent session, alerts and prioritizer threads), `Torrent`, `File`, `Reader`
  (a stream connection), `memory_storage`, NAT-PMP, `torrent_state.h` (Elementum's torrent states)
- `src/web/` - oatpp controllers, one header and one .cpp each. `controller.h` has the shared error handling
- `src/dto/` - oatpp DTOs, the JSON of the API (and its Swagger description)
- `src/utils/` - small helpers, `src/external/` - submodules, do not edit
- `web/` - Web UI: TypeScript, React and Mantine in `web/src/`, built by Vite into `web/dist/`
- `web/public/i18n/<code>.json` - translations of the Web UI and of the server (`/info`, `state_localized`)
- `tests/` - unit tests, end-to-end tests and the translation check
- `cmake/`, `docker/`, `scripts/` - build. CMake embeds `changelog.md`, the version and `web/dist/` into generated sources

## Build and test

- `make local-env` once (copies the dependencies from the `elementumorg/lt2http:linux-x64` image), then `make local`:
  native build in `build/cmake/local/lt2http`
- `make <platform>`, like `make linux-x64`: release build in Docker. `make help` lists all targets
- `make test`: unit tests in Docker, then `tests/smoke_test.sh` (no network). `tests/smoke_test.sh <binary>` and
  `tests/public_test.sh <binary>` (Big Buck Bunny from the internet, `make test-public`) run against any build
- `make lint`: clang-format 19, shellcheck and `tests/check_i18n.py`. `make format` fixes the C++ formatting
- `make web` builds the Web UI, `make web-check` checks types, formatting and that `web/dist/` is up to date
- `cd web && npm run dev` serves the Web UI with live reload, using the API of an lt2http on port 65225

Run the unit, smoke and lint checks before calling a change done, and the public test when torrent handling changes.

## Conventions

- Keep changes small and in the style of the surrounding code. clang-format decides the C++ formatting (140 columns),
  prettier the TypeScript (120 columns)
- Comments say why, in one line, not what the code does
- Threads: HTTP threads, the alerts thread and the prioritizer thread share torrents. Torrent state for reading comes
  from `Torrent::snapshot()`, libtorrent handle calls go through `with_handle()`, other shared members have their own
  mutex. Read the comments in `torrent.h` and `session.h` before adding state
- libtorrent applies file priorities later (in its disk thread), so a read right after setting them can be old
- API: endpoints use the `torrentOperation`/`fileOperation`/`operation` helpers of `controller.h` for errors, DTOs
  describe every field with `DTO_FIELD_INFO`. `state_string` is the same in every language, `state_localized` follows
  the `lang` parameter or the `language` setting
- Never log or return secrets: use `Config::masked()` to show settings
- Translations: add a key to `web/public/i18n/en_gb.json` and to all other languages (the 22 of Elementum), with the
  same `{0}` placeholders. `tests/check_i18n.py` fails on missing keys and on keys used in code but not translated.
  Server texts use `lh::translate()`, the Web UI `t()` from `useI18n()`
- Web UI: `web/src/api.ts` has the types of the API, keep them in line with `src/dto/`. After changing `web/src/` or the
  translations, run `make web` and commit `web/dist/` with the change: the C++ build embeds it and does not run Node.js
- User facing changes get a line in `changelog.md`, in the section of the next version
- Do not edit `src/external/`, `web/dist/` (built) or generated files in `build/`
