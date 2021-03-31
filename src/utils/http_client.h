#pragma once

#include <string>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <utils/http_url.h>
#include <utils/strings.h>

httplib::Result do_request(const std::string &uri) {
    HTTPURL u(uri);
    std::string req_path = u.path + "?" + u.query;

    return httplib::Client(Fmt("%s://%s:%d", u.protocol.c_str(), u.domain.c_str(), u.port).c_str()).Get(req_path.c_str());
}