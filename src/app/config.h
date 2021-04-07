#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <json_struct.h>

#include <utils/exceptions.h>

namespace lh {

static std::string VERSION = "0.0.1";

const int file_readahead_pieces = 20;
const std::int64_t memory_size_min = 40 * 1024 * 1024;
const std::int64_t memory_size_max = 300 * 1024 * 1024;
const std::int64_t disk_cache_size = 12 * 1024 * 1024;

extern std::int64_t memory_size;
extern std::string web_interface;
extern int web_port;
extern std::atomic<bool> is_closing;

JS_ENUM(proxy_type_t, none, socks4, socks5, socks5_pw, http, http_pw, i2p_proxy);

JS_ENUM(encryption_type_t, disabled, enabled, forced);

JS_ENUM(storage_type_t, automatic, file, memory);

JS_ENUM(auto_memory_strategy_type_t, min, standard, max);

JS_ENUM(user_agent_type_t, lt2http, transmission_1_9_3, transmission_2_9_2, libtorrent_1_1_0, libtorrent_1_2_0, bittorrent_7_4_3,
        bittorrent_7_5_0, utorrent_2_2_1, utorrent_3_2_0, utorrent_3_4_9, deluge_1_3_6_0, deluge_1_3_12_0, vuze_5_7_3_0)

inline char const *storage_to_string(lh::storage_type_t s) {
    switch (s) {
    case lh::storage_type_t::automatic:
        return "Automatic";
    case lh::storage_type_t::file:
        return "File";
    case lh::storage_type_t::memory:
        return "Memory";
    default:
        return "<>";
    }
}

struct Config {
  public:
    std::string config_file;
    std::string write_config_file;

    std::string web_interface = "0.0.0.0";
    int web_port = 65225;

    storage_type_t download_storage = storage_type_t::memory;

    bool auto_memory_size = true;
    auto_memory_strategy_type_t auto_memory_size_strategy = auto_memory_strategy_type_t::standard;
    int memory_size = 100;
    bool auto_adjust_memory_size = true;

    std::string download_path = ".";
    std::string torrents_path = ".";

    int readahead_percents = 80;

    int buffer_size = 20;
    int end_buffer_size = 4;
    int buffer_timeout = 60;

    int max_upload_rate = 0;
    int max_download_rate = 0;
    bool limit_after_buffering = false;

    bool autoload_torrents = true;
    bool autoload_torrents_paused = true;

    int connections_limit = 0;
    bool conntracker_limit_auto = true;
    int conntracker_limit = 0;

    bool seed_forever = false;
    int share_ratio_limit = 200;
    int seed_time_ratio_limit = 700;
    int seed_time_limit = 24;

    bool disable_dht = false;
    bool disable_tcp = false;
    bool disable_utp = false;
    bool disable_upnp = false;
    bool disable_lsd = false;

    bool listen_auto_detect_ip = true;
    std::vector<std::string> listen_interfaces;
    std::vector<std::string> outgoing_interfaces;

    bool listen_auto_detect_port = true;
    int listen_port_min = 6891;
    int listen_port_max = 6899;

    int magnet_resolve_timeout = 40;

    bool tuned_storage = true;
    int disk_cache_size = 12;

    int session_save = 15;
    encryption_type_t encryption_policy = encryption_type_t::enabled;
    user_agent_type_t spoof_user_agent = user_agent_type_t::libtorrent_1_2_0;

    bool proxy_enabled = false;
    proxy_type_t proxy_type = proxy_type_t::none;
    std::string proxy_host;
    int proxy_port = 1080;
    std::string proxy_login;
    std::string proxy_password;
    bool use_proxy_tracker = true;
    bool use_proxy_download = true;

    bool use_libtorrent_logging = false;

    Config() = default;
    Config(int &argc, char *argv[]);

    JS_OBJECT(JS_MEMBER(web_interface), JS_MEMBER(web_port),

              JS_MEMBER(download_storage), JS_MEMBER(auto_memory_size), JS_MEMBER(auto_memory_size_strategy),
              JS_MEMBER(memory_size), JS_MEMBER(auto_adjust_memory_size),

              JS_MEMBER(download_path), JS_MEMBER(torrents_path),

              JS_MEMBER(readahead_percents),

              JS_MEMBER(buffer_size), JS_MEMBER(end_buffer_size), JS_MEMBER(buffer_timeout),

              JS_MEMBER(max_upload_rate), JS_MEMBER(max_download_rate), JS_MEMBER(limit_after_buffering),

              JS_MEMBER(autoload_torrents), JS_MEMBER(autoload_torrents_paused),

              JS_MEMBER(connections_limit), JS_MEMBER(conntracker_limit_auto), JS_MEMBER(conntracker_limit),

              JS_MEMBER(seed_forever), JS_MEMBER(share_ratio_limit), JS_MEMBER(seed_time_ratio_limit), JS_MEMBER(seed_time_limit),

              JS_MEMBER(disable_dht), JS_MEMBER(disable_tcp), JS_MEMBER(disable_utp), JS_MEMBER(disable_upnp), JS_MEMBER(disable_lsd),

              JS_MEMBER(listen_auto_detect_ip), JS_MEMBER(listen_interfaces), JS_MEMBER(outgoing_interfaces),

              JS_MEMBER(listen_auto_detect_port), JS_MEMBER(listen_port_min), JS_MEMBER(listen_port_max),

              JS_MEMBER(magnet_resolve_timeout),

              JS_MEMBER(tuned_storage), JS_MEMBER(disk_cache_size),

              JS_MEMBER(session_save), JS_MEMBER(encryption_policy), JS_MEMBER(spoof_user_agent),

              JS_MEMBER(proxy_enabled), JS_MEMBER(proxy_type), JS_MEMBER(proxy_host), JS_MEMBER(proxy_port),
              JS_MEMBER(proxy_login), JS_MEMBER(proxy_password), JS_MEMBER(use_proxy_tracker), JS_MEMBER(use_proxy_download),

              JS_MEMBER(use_libtorrent_logging));
};

// Helper template to convert enum to string and vice versa.
template <typename T, typename F> static inline T from_string(const std::string &str) {
    auto &strings = F::strings();
    for (size_t i = 0; i < strings.size(); i++) {
        const JS::DataRef &ref = strings[i];
        if (ref.size == str.size() && memcmp(ref.data, str.c_str(), ref.size) == 0) {
            return static_cast<T>(i);
        }
    }

    throw ParseException("Cannot parse '" + str + "' into '" + typeid(T).name() + "' type");
}

template <typename T, typename F> static inline std::string to_string(const T &from_type) {
    auto i = static_cast<size_t>(from_type);
    return F::strings()[i];
}

} // namespace lh

JS_ENUM_NAMESPACE_DECLARE_STRING_PARSER(lh, proxy_type_t);
JS_ENUM_NAMESPACE_DECLARE_STRING_PARSER(lh, encryption_type_t);
JS_ENUM_NAMESPACE_DECLARE_STRING_PARSER(lh, storage_type_t);
JS_ENUM_NAMESPACE_DECLARE_STRING_PARSER(lh, auto_memory_strategy_type_t);
JS_ENUM_NAMESPACE_DECLARE_STRING_PARSER(lh, user_agent_type_t);
