#include "config.h"
#include "utils/numbers.h"

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <oatpp/network/Server.hpp>

#include <iostream>
#include <fstream>

#include <utils/system.h>

namespace po = boost::program_options;

namespace lh {

namespace {

// Option stored in a member, with the member's value as default.
template <typename T> po::typed_value<T> *bound(T &value) { return po::value<T>(&value)->default_value(value); }

bool is_set(const po::variables_map &vm, const char *name) { return vm.count(name) && !vm[name].defaulted(); }

// Parses the option string into an enum member, if the option was given.
template <typename StringStruct, typename T>
void set_enum(const po::variables_map &vm, const char *name, const std::string &arg, T &target) {
    if (is_set(vm, name))
        target = from_string<T, StringStruct>(arg);
}

} // namespace

std::int64_t memory_size = 0;

std::string web_interface;
std::string web_login;
std::string web_password;
int web_port = 0;

std::atomic<bool> is_closing(false);


Config::Config(int &argc, char *argv[]) {
    // Global options that influence configuration processing
    po::options_description generic("Global options");
    generic.add_options()
        ("version,v",       "print version")
        ("help,h",          "print help")
        ("config,c",        bound(config_file),
                            "path to lt2http.json file")
        ("write,w",         bound(write_config_file),
                            "write current configuration to specific file")
        ("update,u",        "update config json file after initialization")
        ;

    // Custom variables to store settings, that will need a conversion later
    std::string download_storage_arg;
    std::string listen_interfaces_arg;
    std::string outgoing_interfaces_arg;
    std::string auto_memory_size_strategy_arg;
    std::string encryption_policy_arg;
    std::string spoof_user_agent_arg;
    std::string proxy_type_arg;

    // All configuration setting should be there
    po::options_description config("Configuration");
    config.add_options()
        ("web_interface",                   bound(web_interface),
                                            "Select which interface to listen for HTTP server")
        ("web_login",                       bound(web_login),
                                            "Select which login to use for for HTTP server authentication")
        ("web_password",                    bound(web_password),
                                            "Select which password to use for HTTP server authentication")
        ("web_port",                        bound(web_port),
                                            "Select which port to listen for HTTP server")

        ("download_storage",                bound(download_storage_arg),
                                            "Storage type for downloads")
        ("auto_memory_size",                bound(auto_memory_size),
                                            "Automatically decide memory size")
        ("auto_memory_size_strategy",       bound(auto_memory_size_strategy_arg),
                                            "Strategy for automatic memory sizing")
        ("memory_size", bound(memory_size),
                                            "Memory size for memory storage (in MB)")
        ("auto_adjust_memory_size",         bound(auto_adjust_memory_size),
                                            "Automatically increase memory size, depending on file size")

        ("download_path",                   bound(download_path),
                                            "Folder to use for storing downloaded files (for File storage)")
        ("torrents_path",                   bound(torrents_path),
                                            "Folder to use for storing active torrent files and fastresume files (for File storage)")

        ("readahead_percents",              bound(readahead_percents),
                                            "Percentage to use for upcoming downloads. Rest is used for backward seek (Value: 1 to 100)")

        ("buffer_size",                     bound(buffer_size),
                                            "Buffer size, to download before file is ready for playback (in MB)")
        ("end_buffer_size",                 bound(end_buffer_size),
                                            "End-of-file buffer size, to download before file is ready for playback (in MB)")
        ("buffer_timeout",                  bound(buffer_timeout),
                                            "Seconds to wait for buffer download (in seconds)")

        ("max_upload_rate",                 bound(max_upload_rate),
                                            "Limit upload speed (in KB)")
        ("max_download_rate",               bound(max_download_rate),
                                            "Limit download speed (in KB)")
        ("limit_after_buffering",           bound(limit_after_buffering),
                                            "Use speed limiting only after buffer is downloaded")

        ("autoload_torrents",               bound(autoload_torrents),
                                            "Automatically load previous torrents where application is started")
        ("autoload_torrents_paused",        bound(autoload_torrents_paused),
                                            "Pause automatically loaded previous torrents after adding")

        ("connections_limit",               bound(connections_limit),
                                            "Limit connections per torrent")
        ("conntracker_limit_auto",          bound(conntracker_limit_auto),
                                            "Automatically set connection attempts")
        ("conntracker_limit",               bound(conntracker_limit),
                                            "Connection attempts per second")

        ("seed_forever",                    bound(seed_forever),
                                            "Do NOT stop seeding automatically")
        ("share_ratio_limit",               bound(share_ratio_limit),
                                            "Stop seeding when this byte ratio is met")
        ("seed_time_ratio_limit",           bound(seed_time_ratio_limit),
                                            "Stop seeding when this time ratio is met")
        ("seed_time_limit",                 bound(seed_time_limit),
                                            "Stop seeding after this time spent on seeding (in  hours)")

        ("disable_dht",                     bound(disable_dht),
                                            "Disable DHT")
        ("disable_tcp",                     bound(disable_tcp),
                                            "Disable TCP")
        ("disable_utp",                     bound(disable_utp),
                                            "Disable UTP")
        ("disable_upnp",                    bound(disable_upnp),
                                            "Disable UPNP")
        ("disable_lsd",                     bound(disable_lsd),
                                            "Disable LSD")

        ("listen_auto_detect_ip",           bound(listen_auto_detect_ip),
                                            "Automatically detect listen interface")
        ("listen_interfaces",               bound(listen_interfaces_arg),
                                            "Listen interfaces to use")
        ("outgoing_interfaces",             bound(outgoing_interfaces_arg),
                                            "Outgoing interfaces to use")

        ("listen_auto_detect_port",         bound(listen_auto_detect_port),
                                            "Automatically select ports for listening")
        ("listen_port_min",                 bound(listen_port_min),
                                            "Select minimal port number for range")
        ("listen_port_max",                 bound(listen_port_max),
                                            "Select maximum port number for range")

        ("magnet_resolve_timeout",          bound(magnet_resolve_timeout),
                                            "Timeout for resolving magnet (in seconds)")

        ("tuned_storage",                   bound(tuned_storage),
                                            "Use tuned File storage (to lower random IO writes)")
        ("disk_cache_size",                 bound(disk_cache_size),
                                            "Cache size use for File storage (in MB)")

        ("session_save",                    bound(session_save),
                                            "Seconds between session backup")
        ("encryption_policy",               bound(encryption_policy_arg),
                                            "Encryption policy")
        ("spoof_user_agent",                bound(spoof_user_agent_arg),
                                            "Custom user agent for libtorrent")

        ("proxy_enabled",                   bound(proxy_enabled),
                                            "Enable proxy")
        ("proxy_type",                      bound(proxy_type_arg),
                                            "Proxy type")
        ("proxy_host",                      bound(proxy_host),
                                            "Proxy host")
        ("proxy_port",                      bound(proxy_port),
                                            "Proxy port")
        ("proxy_login",                     bound(proxy_login),
                                            "Proxy login")
        ("proxy_password",                  bound(proxy_password),
                                            "Proxy password")
        ("use_proxy_tracker",               bound(use_proxy_tracker),
                                            "Use proxy for requesting trackers")
        ("use_proxy_download",              bound(use_proxy_download),
                                            "Use proxy for downloading data")

        ("use_libtorrent_logging",          bound(use_libtorrent_logging),
                                            "Enable full libtorrent logging (log all events)")
        ;

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config);

    po::variables_map vm;
    store(po::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << cmdline_options << "\n";
        exit(0);
    }

    if (vm.count("version")) {
        std::cout << "Version: " << lh::VERSION;
        exit(0);
    }

    if (vm.count("update")) {
        update_file = true;
    }

    // Read configuration from file
    if (is_set(vm, "config")) {
        std::ifstream ifs(config_file.c_str());
        if (!ifs && !update_file) {
            OATPP_LOGE("Config", "Cannot open config file: %s", config_file.c_str());
            throw std::invalid_argument("Cannot open configuration file:" + config_file);
        } 
        if (ifs) {
            OATPP_LOGI("Config", "Reading configuration from file: %s", config_file.c_str());

            std::string json_data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            JS::ParseContext context(json_data);

            auto err = context.parseTo(*this);

            if (err != JS::Error::NoError) {
                std::string errorStr = context.makeErrorString();
                OATPP_LOGE("Config", "Error converting json: %s", errorStr.c_str());
                throw std::invalid_argument("Error converting json: " + errorStr);
            }
        }
    } else {
        update_file = false;
    }

    // Convert string variables into enum types
    set_enum<js_storage_type_t_string_struct>(vm, "download_storage", download_storage_arg, download_storage);
    if (is_set(vm, "listen_interfaces")) {
        boost::algorithm::split(listen_interfaces, listen_interfaces_arg, boost::is_any_of(", "), boost::token_compress_on);
    }
    if (is_set(vm, "outgoing_interfaces")) {
        boost::algorithm::split(outgoing_interfaces, outgoing_interfaces_arg, boost::is_any_of(", "), boost::token_compress_on);
    }
    set_enum<js_auto_memory_strategy_type_t_string_struct>(vm, "auto_memory_size_strategy", auto_memory_size_strategy_arg,
                                                           auto_memory_size_strategy);
    set_enum<js_encryption_type_t_string_struct>(vm, "encryption_policy", encryption_policy_arg, encryption_policy);
    set_enum<js_user_agent_type_t_string_struct>(vm, "spoof_user_agent", spoof_user_agent_arg, spoof_user_agent);
    set_enum<js_proxy_type_t_string_struct>(vm, "proxy_type", proxy_type_arg, proxy_type);

    // Write current configuration to desired file
    if (is_set(vm, "write")) {
        std::ofstream ofs(write_config_file.c_str());
        if (!ofs) {
            OATPP_LOGE("Config", "Cannot open output file: %s", write_config_file.c_str());
            throw std::invalid_argument("Cannot open output file:" + write_config_file);
        } 

        OATPP_LOGI("Config", "Writing current configuration to file: %s", write_config_file.c_str());

        ofs << JS::serializeStruct(*this);
        ofs.close();
        std::exit(0);
    }

    // Write current configuration to config file, used for loading.
    if (vm.count("update") && update_file) {
        std::ofstream ofs(config_file.c_str());
        if (!ofs) {
            OATPP_LOGE("Config", "Cannot open output file: %s", config_file.c_str());
            throw std::invalid_argument("Cannot open output file:" + config_file);
        } 

        OATPP_LOGI("Config", "Writing current configuration to file: %s", config_file.c_str());

        ofs << JS::serializeStruct(*this);
        ofs.close();
    }

    // Autoselect memory size
    if (auto_memory_size) {
        if (auto_memory_size_strategy == auto_memory_strategy_type_t::min) {
            memory_size = memory_size_min / 1024 / 1024;
        } else {
            // We try to use 15 percent of physical memory size for MAX strategy and 8 percent for usual strategy.
            int pct = auto_memory_size_strategy == auto_memory_strategy_type_t::max ? 15 : 8;

            std::int64_t total = get_total_system_memory();
            std::int64_t mem = total / 100 * pct;
            if (mem > 0 && total > 0) {
                memory_size = int(mem / 1024 / 1024);
            }

            OATPP_LOGI("Config", "Total system memory: %s", humanize_bytes(total).c_str());
            OATPP_LOGI("Config", "Automatically selected memory size: %s", humanize_bytes(std::int64_t(memory_size)*1024*1024).c_str());

            if (std::int64_t(memory_size)*1024*1024 > memory_size_max) {
                OATPP_LOGI("Config", "Selected memory size (%s) is bigger than maximum for auto-select (%s), so we decrease memory size to maximum allowed: %s", 
                    humanize_bytes(std::int64_t(memory_size)*1024*1024).c_str(), humanize_bytes(memory_size_max).c_str(), humanize_bytes(memory_size_max).c_str());
                memory_size = memory_size_max / 1024/1024;
            }
        }
    }

    OATPP_LOGI("Config", "Active configuration:\n%s", JS::serializeStruct(*this).c_str());
}
}