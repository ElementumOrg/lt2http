#pragma once

#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "libtorrent/aux_/escape_string.hpp"
#include "libtorrent/sha1_hash.hpp"

template <typename... Args> std::string Fmt(const std::string &format, Args... args) {
    int size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size <= 0) {
        throw std::runtime_error("Error during formatting");
    }
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

template <typename T> std::string join(const T &v, const std::string &delim) {
    std::ostringstream s;
    for (const auto &i : v) {
        if (&i != &v[0]) {
            s << delim;
        }
        s << i;
    }
    return s.str();
}

inline std::string uri_escape(const std::string& input) { return lt::escape_string(boost::string_view(input.c_str(), input.size())); }

inline std::string uri_unescape(const std::string& input) {
    lt::error_code ec;
    return lt::unescape_string(boost::string_view(input.c_str(), input.size()), ec);
}

inline std::string to_hex(lt::sha1_hash const &s) {
    std::stringstream ret;
    ret << s;
    return ret.str();
}

inline std::string get_random_numeric(int size) {
    std::string possible_characters = "0123456789";
    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<> dist(0, possible_characters.size() - 1);
    std::string ret;
    for (int i = 0; i < size; i++) {
        int random_index = dist(engine);
        ret += possible_characters[random_index];
    }
    return ret;
}

inline std::string peer_num_to_string(int num) {
    if (num <= 0)
        return "-";

    return std::to_string(num);
}

inline std::string get_file_ext(const std::string &s) {
    std::size_t i = s.rfind('.', s.length());
    if (i != std::string::npos) {
        return (s.substr(i + 1, s.length() - i));
    }

    return ("");
}