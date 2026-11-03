#pragma once

#include <string>

// Defined in build_info.cpp, which CMake generates: a new version or changelog recompiles only that file.
namespace lh {
extern const std::string VERSION;
}

// changelog.md, served by /changelog
extern const char *const lt2http_changelog;
