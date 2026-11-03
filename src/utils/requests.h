#pragma once

#include <map>
#include <string>

#include "strings.h"

inline std::string guess_mime_type(const std::string &file) {
    static const std::map<std::string, std::string> types = {
        {"mp4", "video/mp4"},        {"m4v", "video/x-m4v"},     {"mkv", "video/x-matroska"}, {"webm", "video/webm"},
        {"mov", "video/quicktime"},  {"avi", "video/x-msvideo"}, {"wmv", "video/x-ms-wmv"},   {"mpg", "video/mpeg"},
        {"flv", "video/x-flv"},      {"3gp", "video/3gpp"},
    };

    auto type = types.find(get_file_ext(file));
    return type == types.end() ? "" : type->second;
}
