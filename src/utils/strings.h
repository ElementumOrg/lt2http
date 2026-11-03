#pragma once

#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>

#include <libtorrent/aux_/escape_string.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/sha1_hash.hpp>

template <typename... Args> std::string Fmt(const std::string &format, Args... args) {
    int size = snprintf(nullptr, 0, format.c_str(), args...);
    if (size < 0)
        throw std::runtime_error("Error during formatting");

    std::string result(size, '\0');
    snprintf(&result[0], size + 1, format.c_str(), args...);
    return result;
}

template <typename T> std::string join(const T &values, const std::string &delim) {
    std::string result;
    for (const auto &value : values) {
        if (!result.empty())
            result += delim;
        result += value;
    }
    return result;
}

inline std::string uri_escape(const std::string &input) { return lt::escape_string(input); }

inline std::string uri_unescape(const std::string &input) {
    lt::error_code ec;
    return lt::unescape_string(input, ec);
}

inline std::string to_hex(lt::sha1_hash const &s) { return lt::aux::to_hex(s); }

inline std::string get_random_numeric(int size) {
    thread_local std::mt19937 engine{std::random_device{}()};
    std::uniform_int_distribution<int> digit(0, 9);
    std::string result;
    for (int i = 0; i < size; i++)
        result += char('0' + digit(engine));
    return result;
}

inline std::string peer_num_to_string(int num) { return num <= 0 ? "-" : std::to_string(num); }

inline std::string get_file_ext(const std::string &s) {
    auto i = s.rfind('.');
    return i == std::string::npos ? "" : s.substr(i + 1);
}
