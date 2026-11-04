#include "http_client.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

std::optional<HttpResponse> do_request(const std::string &uri, const std::vector<std::pair<std::string, std::string>> &headers) {
    // httplib takes "scheme://host:port" and the path separately.
    auto scheme_end = uri.find("://");
    auto path_start = uri.find('/', scheme_end == std::string::npos ? 0 : scheme_end + 3);
    auto path = path_start == std::string::npos ? "/" : uri.substr(path_start);

    httplib::Client cli(uri.substr(0, path_start));

    cli.enable_server_certificate_verification(false);
    cli.set_follow_location(true);
    cli.set_compress(true);
    // The session waits for this request on shutdown, httplib waits 300s to connect by default.
    cli.set_connection_timeout(10);
    cli.set_read_timeout(30);

    auto res = cli.Get(path.substr(0, path.find('#')), httplib::Headers(headers.begin(), headers.end()));
    if (!res)
        return std::nullopt;
    return HttpResponse{res->status, std::move(res->body)};
}
