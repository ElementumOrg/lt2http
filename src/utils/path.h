#pragma once

#include <boost/filesystem/operations.hpp>
#include <ctime>
#include <iostream>
#include <fstream>
#include <ios>
#include <string>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#include <direct.h> // for _mkdir and _getcwd
#include <windows.h>
#include <conio.h>

#else

#include <termios.h>
#include <sys/ioctl.h>
#include <csignal>
#include <utility>
#include <dirent.h>

#endif

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include <libtorrent/error_code.hpp>
#include <libtorrent/string_view.hpp>

inline bool file_exists(const std::string &name) {
    std::ifstream f(name.c_str());
    return f.good();
}

inline int save_file(std::string const &filename, char const *v, std::size_t s) {
    std::fstream f(filename, std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
    f.write(v, s);
    return !f.fail();
}

inline int save_file(std::string const &filename, std::vector<char> const &v) {
    std::fstream f(filename, std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
    f.write(v.data(), v.size());
    return !f.fail();
}

inline bool load_file(std::string const &filename, std::string &v) {
    if (!file_exists(filename))
        return false;

    std::fstream f(filename, std::ios_base::in | std::ios_base::binary);
    f.seekg(0, std::ios_base::end);
    auto const s = f.tellg();
    v.reserve(s);
    f.seekg(0, std::ios_base::beg);
    if (s == std::fstream::pos_type(0))
        return !f.fail();
    v.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return !f.fail();
}

inline bool load_file(std::string const &filename, std::vector<char> &v, int limit = 8000000) {
    if (!file_exists(filename))
        return false;

    std::fstream f(filename, std::ios_base::in | std::ios_base::binary);
    f.seekg(0, std::ios_base::end);
    auto const s = f.tellg();
    if (s > limit || s < 0)
        return false;
    f.seekg(0, std::ios_base::beg);
    v.resize(static_cast<std::size_t>(s));
    if (s == std::fstream::pos_type(0))
        return !f.fail();
    f.read(v.data(), v.size());
    return !f.fail();
}

inline bool is_absolute_path(std::string const &f) {
    if (f.empty())
        return false;
    return boost::filesystem::path{f}.is_absolute();
}

inline std::string path_append(std::string const &lhs, std::string const &rhs) {
    if (lhs.empty() || lhs == ".")
        return rhs;
    if (rhs.empty() || rhs == ".")
        return lhs;

    return boost::filesystem::path{lhs}.append(rhs).string();
}

inline std::string make_absolute_path(std::string const &p) {
    if (is_absolute_path(p))
        return p;
    
    return boost::filesystem::absolute(boost::filesystem::path{p}).string();
}

inline std::string resume_file(std::string save_path, std::string info_hash) {
    return path_append(save_path, info_hash + ".resume");
}

inline std::string parts_file(std::string save_path, std::string info_hash) {
    return path_append(save_path, info_hash + ".parts");
}

inline std::string torrent_file(std::string save_path, std::string info_hash) {
    return path_append(save_path, info_hash + ".torrent");
}

inline std::multimap<std::time_t, boost::filesystem::path> list_dir(std::string path, bool (*filter_fun)(lt::string_view), boost::system::error_code &ec) {
    boost::filesystem::path p(path);

    std::multimap<std::time_t, boost::filesystem::path> files;

    if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p))
        return files;

    for (boost::filesystem::directory_entry& entry : boost::filesystem::directory_iterator(p, ec)) {
        if (boost::filesystem::is_regular_file(entry) && filter_fun(entry.path().filename().string()))
            files.insert(std::pair<std::time_t, boost::filesystem::path>(boost::filesystem::last_write_time(entry), entry.path()));
    }

    return files;
}

inline void remove_file(std::string &path) {
    if (!file_exists(path))
        return;

    std::remove(path.c_str());
};

inline std::time_t modtime_file(std::string &path) {
    return boost::filesystem::last_write_time(path);
}

inline boost::system::error_code mkpath(std::string &path) {
    boost::system::error_code ec;
    boost::filesystem::create_directories(path, ec);
    return ec;
};
