// Unit tests of the helpers, built with -Dbuild_tests=ON and run with ctest (make test does both).
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <array>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/filesystem.hpp>

#include <app/build_info.h>

#include <app/config.h>
#include <app/i18n.h>
#include <bittorrent/natpmp.h>
#include <bittorrent/torrent_state.h>
#include <utils/numbers.h>
#include <utils/path.h>
#include <utils/pieces.h>
#include <utils/requests.h>
#include <utils/strings.h>
#include <web/controller.h>

static int failures = 0;

// Not assert(): it is compiled out in release builds
#define CHECK(expr)                                                                                                                        \
    do {                                                                                                                                   \
        if (!(expr)) {                                                                                                                     \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr);                                                  \
            failures++;                                                                                                                    \
        }                                                                                                                                  \
    } while (0)

static void test_changelog() {
    // Served by /changelog from the binary, there is no file to read at runtime
    std::ifstream file(LT2HTTP_CHANGELOG_FILE, std::ios::binary);
    std::stringstream content;
    content << file.rdbuf();
    CHECK(!content.str().empty());
    CHECK(content.str() == lt2http_changelog);
}

static void test_numbers() {
    CHECK(humanize_bytes(0) == "0 B");
    CHECK(humanize_bytes(1536) == "1.5 KB");
    CHECK(humanize_bytes(3000000) == "2.86 MB");
    CHECK(humanize_bytes(-1) == "-1");

    CHECK(progression_result(2, 1.7, 1) == 2);
    CHECK(progression_result(2, 1.7, 2) == 3);

    CHECK(random(5, 5) == 5);
    for (int i = 0; i < 100; i++) {
        int value = random(6891, 6899);
        CHECK(value >= 6891 && value <= 6899);
    }
}

static void test_strings() {
    CHECK(Fmt("%s-%d", "a", 1) == "a-1");
    CHECK(Fmt("%s", "") == "");
    CHECK(join(std::vector<std::string>{"a", "b", "c"}, ",") == "a,b,c");
    CHECK(join(std::vector<std::string>{}, ",") == "");

    std::string name = "Movie name [1080p] & more.mkv";
    CHECK(uri_escape(name).find(' ') == std::string::npos);
    CHECK(uri_unescape(uri_escape(name)) == name);

    lt::sha1_hash hash;
    for (int i = 0; i < 20; i++)
        hash[i] = char(i);
    CHECK(to_hex(hash) == "000102030405060708090a0b0c0d0e0f10111213");

    CHECK(get_file_ext("a.b.mkv") == "mkv");
    CHECK(get_file_ext("noext") == "");
    CHECK(peer_num_to_string(0) == "-");
    CHECK(peer_num_to_string(5) == "5");

    CHECK(guess_mime_type("movie.mkv") == "video/x-matroska");
    CHECK(guess_mime_type("movie.mp4") == "video/mp4");
    CHECK(guess_mime_type("notes.txt") == "");

    CHECK(html_escape(R"(<a href="x">'&'</a>)") == "&lt;a href=&quot;x&quot;&gt;&#39;&amp;&#39;&lt;/a&gt;");

    CHECK(get_random_numeric(10).size() == 10);
    CHECK(get_random_numeric(10).find_first_not_of("0123456789") == std::string::npos);
}

static void test_state() {
    using lh::TorrentState;
    auto state = [](const lt::torrent_status &s, bool metadata = true, bool paused = false, bool buffering = false, int players = 0) {
        return lh::compute_state(s, metadata, paused, buffering, players);
    };

    lt::torrent_status s;
    s.state = lt::torrent_status::downloading;
    s.total_wanted = 100;
    s.total_wanted_done = 10;
    CHECK(state(s) == TorrentState::Stalled);
    s.download_payload_rate = 1000;
    CHECK(state(s) == TorrentState::Downloading);
    CHECK(state(s, false) == TorrentState::Finding);
    CHECK(state(s, true, true) == TorrentState::Paused);
    CHECK(state(s, true, false, true, 1) == TorrentState::Buffering);
    CHECK(state(s, true, false, false, 1) == TorrentState::Playing);

    s.flags = lt::torrent_flags::auto_managed | lt::torrent_flags::paused;
    CHECK(state(s) == TorrentState::Queued);
    s.flags = {};

    s.state = lt::torrent_status::checking_files;
    CHECK(state(s, true, false, true, 1) == TorrentState::Checking);

    s.state = lt::torrent_status::finished;
    s.total_wanted_done = 100;
    CHECK(state(s) == TorrentState::Finished);
    CHECK(state(s, true, true) == TorrentState::Finished);
    s.is_seeding = true;
    CHECK(state(s) == TorrentState::Seeding);

    s.errc = lt::errors::make_error_code(lt::errors::file_too_short);
    CHECK(state(s, true, true) == TorrentState::Error);

    // Elementum's numbers
    CHECK(int(TorrentState::Finding) == 2);
    CHECK(int(TorrentState::Playing) == 10);
    CHECK(std::string(lh::state_to_string(TorrentState::Seeding)) == "Seeding");
}

static void test_i18n() {
    CHECK(lh::languages().size() == 22);
    CHECK(lh::languages().front().code == "de_de");
    CHECK(lh::resolve_language("de") == "de_de");
    CHECK(lh::resolve_language("DE-de") == "de_de");
    CHECK(lh::resolve_language("es") == "es_es");
    CHECK(lh::resolve_language("es-CO") == "es_es");
    CHECK(lh::resolve_language("pt") == "pt_pt");
    CHECK(lh::resolve_language("en") == "en_gb");
    CHECK(lh::resolve_language("xx") == "en_gb");
    CHECK(lh::resolve_language("", "ru_ru") == "ru_ru");

    CHECK(lh::translate("en_gb", "state.Seeding") == "Seeding");
    CHECK(lh::translate("de_de", "state.Seeding") == "Seeden");
    CHECK(lh::translate("xx_xx", "state.Seeding") == "Seeding");
    CHECK(lh::translate("de_de", "no.such_key") == "no.such_key");
    CHECK(lh::translate("en_gb", "tracker.failed", {"3"}) == "Failures: 3");

    CHECK(lh::find_web_file("index.html") != nullptr);
    CHECK(lh::find_web_file("assets/index.js") != nullptr);
    CHECK(lh::find_web_file("i18n/en_gb.json") != nullptr);
    CHECK(lh::find_web_file("nope.html") == nullptr);

    CHECK(pad_right("ab", 4) == "ab  ");
    CHECK(pad_right("Größe", 6) == "Größe ");
    CHECK(pad_right("abcdef", 3) == "abcdef ");
}

static void test_url_headers() {
    auto [url, headers] = split_url_headers("https://host/a.torrent|Cookie=a=b; c=d|User-Agent=Kodi");
    CHECK(url == "https://host/a.torrent");
    CHECK(headers.size() == 2);
    CHECK((headers[0] == std::pair<std::string, std::string>{"Cookie", "a=b; c=d"}));
    CHECK((headers[1] == std::pair<std::string, std::string>{"User-Agent", "Kodi"}));
    CHECK(split_url_headers("https://host/a.torrent").second.empty());

    bool threw = false;
    try {
        split_url_headers("https://host/a.torrent|nope");
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    CHECK(threw);
}

static void test_masked_config() {
    lh::Config config;
    config.web_password = "secret";
    config.proxy_password = "secret";
    auto json = JS::serializeStruct(config.masked());
    CHECK(json.find("secret") == std::string::npos);
    CHECK(config.masked().web_password == "***");
    CHECK(lh::Config().masked().web_password.empty());
}

static void test_pieces() {
    auto region = [](std::int64_t offset, std::int64_t size, int piece_length, int pieces_count) {
        auto [first, last] = region_pieces(offset, size, piece_length, pieces_count);
        return std::pair<int, int>{int(first), int(last)};
    };
    CHECK((region(0, 16384, 16384, 10) == std::pair<int, int>{0, 0}));
    CHECK((region(16000, 1000, 16384, 10) == std::pair<int, int>{0, 1}));
    CHECK((region(0, 1000000, 16384, 10) == std::pair<int, int>{0, 9}));
    CHECK((region(0, 100, 0, 10) == std::pair<int, int>{0, 0}));
}

static void test_path() {
    CHECK(path_append(".", "b") == "b");
    CHECK(path_append("a", "") == "a");
    CHECK(boost::filesystem::path(path_append("a", "b")) == boost::filesystem::path("a") / "b");
    CHECK(resume_file("dir", "abc") == path_append("dir", "abc.resume"));

    auto dir = (boost::filesystem::temp_directory_path() / boost::filesystem::unique_path()).string();
    CHECK(!mkpath(path_append(dir, "sub")));
    auto file = path_append(dir, "data.torrent");
    std::string data("binary\0data", 11);
    CHECK(save_file(file, data.data(), data.size()));
    CHECK(file_exists(file));

    std::string loaded;
    CHECK(load_file(file, loaded) && loaded == data);
    std::vector<char> too_small;
    CHECK(!load_file(file, too_small, 5));
    CHECK(!load_file(path_append(dir, "missing"), loaded));

    CHECK(save_file(path_append(dir, "other.txt"), "x", 1));
    boost::system::error_code ec;
    auto files = list_files(dir, ".torrent", ec);
    CHECK(!ec && files.size() == 1 && files.begin()->second.filename() == "data.torrent");

    remove_file(file);
    CHECK(!file_exists(file));
    boost::filesystem::remove_all(dir);
}

static void test_params() {
    CHECK(lh::hash_from_param(" ABCdef%20 ") == "abcdef");
    CHECK(lh::is_true("true"));
    CHECK(!lh::is_true("1"));
    CHECK(!lh::is_true("false"));

    CHECK((lh::from_string<lh::storage_type_t, lh::js_storage_type_t_string_struct>("memory") == lh::storage_type_t::memory));
    bool thrown = false;
    try {
        lh::from_string<lh::storage_type_t, lh::js_storage_type_t_string_struct>("bogus");
    } catch (const lh::ParseException &) {
        thrown = true;
    }
    CHECK(thrown);
}

static void test_natpmp() {
    using boost::asio::ip::address_v4;
    using boost::asio::ip::udp;
    auto ip = [](const char *s) { return boost::asio::ip::make_address_v4(s); };

    // ProtonVPN-like /32 address without a route gateway
    CHECK(lh::natpmp_gateway_guesses(ip("10.2.0.2"), {}) == (std::vector<address_v4>{ip("10.2.0.1"), ip("10.0.0.1")}));
    CHECK(lh::natpmp_gateway_guesses(ip("192.168.1.5"), ip("192.168.1.1")) ==
          (std::vector<address_v4>{ip("192.168.1.1"), ip("192.168.0.1"), ip("192.0.0.1")}));
    CHECK(lh::natpmp_gateway_guesses(ip("10.0.0.1"), {}).empty());

    // A fake gateway on localhost: keeping the port renews the mapping, moving it fails and deletes the unusable mapping
    for (int external : {6891, 7000}) {
        boost::asio::io_context ioc;
        udp::socket server(ioc, udp::endpoint(address_v4::loopback(), 5351));
        std::thread gateway([&] {
            int const requests = external == 6891 ? 2 : 4; // TCP and UDP, then their deletion
            for (int i = 0; i < requests; i++) {
                std::array<unsigned char, 12> req{};
                udp::endpoint from;
                server.receive_from(boost::asio::buffer(req), from);
                std::array<unsigned char, 16> res{0,
                                                  static_cast<unsigned char>(128 + req[1]),
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  req[4],
                                                  req[5],
                                                  static_cast<unsigned char>(external >> 8),
                                                  static_cast<unsigned char>(external),
                                                  req[8],
                                                  req[9],
                                                  req[10],
                                                  req[11]};
                server.send_to(boost::asio::buffer(res), from);
            }
        });

        lh::NatMapping m;
        m.local = m.gateway = address_v4::loopback();
        m.port = 6891;
        std::atomic<bool> const abort{false};
        CHECK(lh::natpmp_refresh(m, abort) == (external == 6891));
        gateway.join();
    }
}

int main() {
    test_changelog();
    test_numbers();
    test_strings();
    test_state();
    test_i18n();
    test_url_headers();
    test_masked_config();
    test_pieces();
    test_path();
    test_params();
    test_natpmp();

    if (failures)
        std::fprintf(stderr, "%d checks failed\n", failures);
    else
        std::printf("All checks passed\n");
    return failures ? 1 : 0;
}
