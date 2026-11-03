// Unit tests of the helpers, built with -Dbuild_tests=ON and run with ctest (make test does both).
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include <app/build_info.h>

#include <app/config.h>
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

    CHECK(get_random_numeric(10).size() == 10);
    CHECK(get_random_numeric(10).find_first_not_of("0123456789") == std::string::npos);
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

int main() {
    test_changelog();
    test_numbers();
    test_strings();
    test_pieces();
    test_path();
    test_params();

    if (failures)
        std::fprintf(stderr, "%d checks failed\n", failures);
    else
        std::printf("All checks passed\n");
    return failures ? 1 : 0;
}
