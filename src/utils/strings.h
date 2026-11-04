#pragma once

#include <cstdarg>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>

#include <libtorrent/aux_/escape_string.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/sha1_hash.hpp>

// printf-style formatting, checked by the compiler like printf
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
inline std::string
Fmt(const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list size_args;
    va_copy(size_args, args);
    int size = vsnprintf(nullptr, 0, format, size_args);
    va_end(size_args);

    std::string result;
    if (size > 0) {
        result.resize(size);
        vsnprintf(&result[0], size + 1, format, args);
    }
    va_end(args);

    if (size < 0)
        throw std::runtime_error("Error during formatting");
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

// Text from torrents and API clients, put into the HTML of /info
inline std::string html_escape(const std::string &input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result += c;
        }
    }
    return result;
}

// Pads with spaces to a width in characters (not UTF-8 bytes), to line up columns of translated text
inline std::string pad_right(const std::string &text, std::size_t width) {
    std::size_t length = 0;
    for (unsigned char c : text)
        length += (c & 0xC0) != 0x80;
    return length >= width ? text + " " : text + std::string(width - length, ' ');
}

inline std::string get_file_ext(const std::string &s) {
    auto i = s.rfind('.');
    return i == std::string::npos ? "" : s.substr(i + 1);
}
