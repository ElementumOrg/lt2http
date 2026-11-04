#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "strings.h"

inline std::string guess_mime_type(const std::string &file) {
    static const std::map<std::string, std::string> types = {
        {"mp4", "video/mp4"},       {"m4v", "video/x-m4v"},     {"mkv", "video/x-matroska"}, {"webm", "video/webm"},
        {"mov", "video/quicktime"}, {"avi", "video/x-msvideo"}, {"wmv", "video/x-ms-wmv"},   {"mpg", "video/mpeg"},
        {"flv", "video/x-flv"},     {"3gp", "video/3gpp"},
    };

    auto type = types.find(get_file_ext(file));
    return type == types.end() ? "" : type->second;
}

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

// "http://host/file.torrent|Cookie=a=b|User-Agent=Kodi" is the url and the headers to send with it
inline std::pair<std::string, HttpHeaders> split_url_headers(const std::string &input) {
    std::vector<std::string> parts;
    boost::split(parts, input, boost::is_any_of("|"));

    HttpHeaders headers;
    for (std::size_t i = 1; i < parts.size(); i++) {
        auto separator = parts[i].find('=');
        if (separator == std::string::npos || separator == 0)
            throw std::invalid_argument("Header '" + parts[i] + "' is not in key=value format");
        headers.emplace_back(boost::trim_copy(parts[i].substr(0, separator)), parts[i].substr(separator + 1));
    }
    return {parts[0], headers};
}
