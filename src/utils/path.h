#pragma once

#include <cstdint>
#include <ctime>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

inline bool file_exists(const std::string &name) {
    boost::system::error_code ec;
    return boost::filesystem::exists(name, ec);
}

[[nodiscard]] inline bool save_file(const std::string &filename, char const *data, std::size_t size) {
    std::ofstream f(filename, std::ios_base::trunc | std::ios_base::binary);
    f.write(data, std::streamsize(size));
    return !f.fail();
}

// Reads a whole file into a std::string or std::vector<char>, fails for files bigger than limit.
template <typename Container> [[nodiscard]] bool load_file(const std::string &filename, Container &v, std::int64_t limit = 8000000) {
    boost::system::error_code ec;
    auto size = boost::filesystem::file_size(filename, ec);
    if (ec || size > std::uintmax_t(limit))
        return false;

    std::ifstream f(filename, std::ios_base::binary);
    v.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.bad();
}

inline std::string path_append(const std::string &lhs, const std::string &rhs) {
    if (lhs.empty() || lhs == ".")
        return rhs;
    if (rhs.empty() || rhs == ".")
        return lhs;

    return (boost::filesystem::path{lhs} / rhs).string();
}

inline std::string resume_file(const std::string &save_path, const std::string &info_hash) {
    return path_append(save_path, info_hash + ".resume");
}

inline std::string parts_file(const std::string &save_path, const std::string &info_hash) {
    return path_append(save_path, info_hash + ".parts");
}

inline std::string torrent_file(const std::string &save_path, const std::string &info_hash) {
    return path_append(save_path, info_hash + ".torrent");
}

// Regular files with the extension (".torrent") in a directory, by modification time.
inline std::multimap<std::time_t, boost::filesystem::path> list_files(const std::string &path, const std::string &extension,
                                                                      boost::system::error_code &ec) {
    std::multimap<std::time_t, boost::filesystem::path> files;

    if (!boost::filesystem::is_directory(path, ec))
        return files;

    for (const auto &entry : boost::filesystem::directory_iterator(path, ec)) {
        if (boost::filesystem::is_regular_file(entry, ec) && entry.path().extension() == extension)
            files.emplace(boost::filesystem::last_write_time(entry, ec), entry.path());
        if (ec)
            return files;
    }

    return files;
}

inline void remove_file(const std::string &path) {
    boost::system::error_code ec;
    boost::filesystem::remove(path, ec);
}

inline std::time_t modtime_file(const std::string &path) { return boost::filesystem::last_write_time(path); }

inline boost::system::error_code mkpath(const std::string &path) {
    boost::system::error_code ec;
    if (!path.empty() && path != ".")
        boost::filesystem::create_directories(path, ec);

    return ec;
}
