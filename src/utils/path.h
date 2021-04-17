#pragma once

#include <iostream>
#include <fstream>
#include <ios>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef TORRENT_WINDOWS
#include <direct.h> // for _mkdir and _getcwd
#include <sys/stat.h>
#include <sys/types.h> // for _stat
#endif

#ifdef _WIN32

#include <windows.h>
#include <conio.h>

#else

#include <termios.h>
#include <sys/ioctl.h>
#include <csignal>
#include <utility>
#include <dirent.h>

#endif

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
#if defined(TORRENT_WINDOWS) || defined(TORRENT_OS2)
    int i = 0;
    // match the xx:\ or xx:/ form
    while (f[i] && strchr("abcdefghijklmnopqrstuvxyz", f[i]))
        ++i;
    if (i < int(f.size() - 1) && f[i] == ':' && (f[i + 1] == '\\' || f[i + 1] == '/'))
        return true;

    // match the \\ form
    if (int(f.size()) >= 2 && f[0] == '\\' && f[1] == '\\')
        return true;
    return false;
#else
    if (f[0] == '/')
        return true;
    return false;
#endif
}

inline std::string path_append(std::string const &lhs, std::string const &rhs) {
    if (lhs.empty() || lhs == ".")
        return rhs;
    if (rhs.empty() || rhs == ".")
        return lhs;

#if defined(TORRENT_WINDOWS) || defined(TORRENT_OS2)
#define TORRENT_SEPARATOR "\\"
    bool need_sep = lhs[lhs.size() - 1] != '\\' && lhs[lhs.size() - 1] != '/';
#else
#define TORRENT_SEPARATOR "/"
    bool need_sep = lhs[lhs.size() - 1] != '/';
#endif
    return lhs + (need_sep ? TORRENT_SEPARATOR : "") + rhs;
}

inline std::string make_absolute_path(std::string const &p) {
    if (is_absolute_path(p))
        return p;
    std::string ret;
#if defined TORRENT_WINDOWS
    char *cwd = ::_getcwd(nullptr, 0);
    ret = path_append(cwd, p);
    std::free(cwd);
#else
    char *cwd = ::getcwd(nullptr, 0);
    ret = path_append(cwd, p);
    std::free(cwd);
#endif
    return ret;
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

inline std::vector<std::string> list_dir(std::string path, bool (*filter_fun)(lt::string_view), lt::error_code &ec) {
    std::vector<std::string> ret;
#ifdef TORRENT_WINDOWS
    if (!path.empty() && path[path.size() - 1] != '\\')
        path += "\\*";
    else
        path += "*";

    WIN32_FIND_DATAA fd;
    HANDLE handle = FindFirstFileA(path.c_str(), &fd);
    if (handle == INVALID_HANDLE_VALUE) {
        ec.assign(GetLastError(), boost::system::system_category());
        return ret;
    }

    do {
        lt::string_view p = fd.cFileName;
        if (filter_fun(p))
            ret.push_back(p.to_string());

    } while (FindNextFileA(handle, &fd));
    FindClose(handle);
#else

    if (!path.empty() && path[path.size() - 1] == '/')
        path.resize(path.size() - 1);

    DIR *handle = opendir(path.c_str());
    if (handle == nullptr) {
        ec.assign(errno, boost::system::system_category());
        return ret;
    }

    struct dirent *de;
    while ((de = readdir(handle))) {
        lt::string_view p(de->d_name);
        if (filter_fun(p))
            ret.push_back(p.to_string());
    }
    closedir(handle);
#endif
    return ret;
}

inline void remove_file(std::string &path) {
    if (!file_exists(path))
        return;

    std::remove(path.c_str());
};

