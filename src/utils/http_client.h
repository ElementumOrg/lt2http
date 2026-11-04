#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

struct HttpResponse {
    int status = 0;
    std::string body;
};

// GET request following redirects, nothing if there is no response.
// Defined in http_client.cpp, so only that file parses cpp-httplib.
std::optional<HttpResponse> do_request(const std::string &uri, const std::vector<std::pair<std::string, std::string>> &headers = {});
