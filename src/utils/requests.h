#pragma once

#include "strings.h"

std::string guess_mime_type(const std::string &file) {
    auto ext = get_file_ext(file);
    if (ext == "mp4")
        return "video/mp4";
    else if (ext == "m4v")
        return "video/x-m4v";
    else if (ext == "mkv")
        return "video/x-matroska";
    else if (ext == "webm")
        return "video/webm";
    else if (ext == "mov")
        return "video/quicktime";
    else if (ext == "avi")
        return "video/x-msvideo";
    else if (ext == "wmv")
        return "video/x-ms-wmv";
    else if (ext == "mpg")
        return "video/mpeg";
    else if (ext == "flv")
        return "video/x-flv";
    else if (ext == "3gp")
        return "video/3gpp";

    return "";
}
