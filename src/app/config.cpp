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

std::int64_t memory_size = 0;
std::string web_interface;
int web_port = 0;

std::atomic<bool> is_closing(false);


Config::Config(int &argc, char *argv[]) {
    // Global options that influence configuration processing
    po::options_description generic("Global options");
    generic.add_options()
        ("version,v",       "print version")
        ("help,h",          "print help")
        ("config,c",        po::value<std::string>(&config_file)->default_value(config_file),
                            "path to lt2http.json file")
        ("write,w",         po::value<std::string>(&write_config_file)->default_value(write_config_file),
                            "write current configuration to specific file")
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
        ("web_interface",                   po::value<std::string>(&web_interface)->default_value(web_interface),
                                            "Select which interface to listen for HTTP server")
        ("web_port",                        po::value<int>(&web_port)->default_value(web_port),
                                            "Select which port to listen for HTTP server")

        ("download_storage",                po::value<std::string>(&download_storage_arg)->default_value(download_storage_arg),
                                            "Storage type for downloads")
        ("auto_memory_size",                po::value<bool>(&auto_memory_size)->default_value(auto_memory_size),
                                            "Automatically decide memory size")
        ("auto_memory_size_strategy",       po::value<std::string>(&auto_memory_size_strategy_arg)->default_value(auto_memory_size_strategy_arg),
                                            "Strategy for automatic memory sizing")
        ("memory_size", po::value<int>(&memory_size)->default_value(memory_size),
                                            "Memory size for memory storage (in MB)")
        ("auto_adjust_memory_size",         po::value<bool>(&auto_adjust_memory_size)->default_value(auto_adjust_memory_size),
                                            "Automatically increase memory size, depending on file size")

        ("download_path",                   po::value<std::string>(&download_path)->default_value(download_path),
                                            "Folder to use for storing downloaded files (for File storage)")
        ("torrents_path",                   po::value<std::string>(&torrents_path)->default_value(torrents_path),
                                            "Folder to use for storing active torrent files and fastresume files (for File storage)")

        ("readahead_percents",              po::value<int>(&readahead_percents)->default_value(readahead_percents),
                                            "Percentage to use for upcoming downloads. Rest is used for backward seek (Value: 1 to 100)")

        ("buffer_size",                     po::value<int>(&buffer_size)->default_value(buffer_size),
                                            "Buffer size, to download before file is ready for playback (in MB)")
        ("end_buffer_size",                 po::value<int>(&end_buffer_size)->default_value(end_buffer_size),
                                            "End-of-file buffer size, to download before file is ready for playback (in MB)")
        ("buffer_timeout",                  po::value<int>(&buffer_timeout)->default_value(buffer_timeout),
                                            "Seconds to wait for buffer download (in seconds)")

        ("max_upload_rate",                 po::value<int>(&max_upload_rate)->default_value(max_upload_rate),
                                            "Limit upload speed (in KB)")
        ("max_download_rate",               po::value<int>(&max_download_rate)->default_value(max_download_rate),
                                            "Limit download speed (in KB)")
        ("limit_after_buffering",           po::value<bool>(&limit_after_buffering)->default_value(limit_after_buffering),
                                            "Use speed limiting only after buffer is downloaded")

        ("autoload_torrents",               po::value<bool>(&autoload_torrents)->default_value(autoload_torrents),
                                            "Automatically load previous torrents where application is started")
        ("autoload_torrents_paused",        po::value<bool>(&autoload_torrents_paused)->default_value(autoload_torrents_paused),
                                            "Pause automatically loaded previous torrents after adding")

        ("connections_limit",               po::value<int>(&connections_limit)->default_value(connections_limit),
                                            "Limit connections per torrent")
        ("conntracker_limit_auto",          po::value<bool>(&conntracker_limit_auto)->default_value(conntracker_limit_auto),
                                            "Automatically set connection attempts")
        ("conntracker_limit",               po::value<int>(&conntracker_limit)->default_value(conntracker_limit),
                                            "Connection attempts per second")

        ("seed_forever",                    po::value<bool>(&seed_forever)->default_value(seed_forever),
                                            "Do NOT stop seeding automatically")
        ("share_ratio_limit",               po::value<int>(&share_ratio_limit)->default_value(share_ratio_limit),
                                            "Stop seeding when this byte ratio is met")
        ("seed_time_ratio_limit",           po::value<int>(&seed_time_ratio_limit)->default_value(seed_time_ratio_limit),
                                            "Stop seeding when this time ratio is met")
        ("seed_time_limit",                 po::value<int>(&seed_time_limit)->default_value(seed_time_limit),
                                            "Stop seeding after this time spent on seeding (in  hours)")

        ("disable_dht",                     po::value<bool>(&disable_dht)->default_value(disable_dht),
                                            "Disable DHT")
        ("disable_tcp",                     po::value<bool>(&disable_tcp)->default_value(disable_tcp),
                                            "Disable TCP")
        ("disable_utp",                     po::value<bool>(&disable_utp)->default_value(disable_utp),
                                            "Disable UTP")
        ("disable_upnp",                    po::value<bool>(&disable_upnp)->default_value(disable_upnp),
                                            "Disable UPNP")

        ("listen_auto_detect_ip",           po::value<bool>(&listen_auto_detect_ip)->default_value(listen_auto_detect_ip),
                                            "Automatically detect listen interface")
        ("listen_interfaces",               po::value<std::string>(&listen_interfaces_arg)->default_value(listen_interfaces_arg),
                                            "Listen interfaces to use")
        ("outgoing_interfaces",             po::value<std::string>(&outgoing_interfaces_arg)->default_value(outgoing_interfaces_arg),
                                            "Outgoing interfaces to use")

        ("listen_auto_detect_port",         po::value<bool>(&listen_auto_detect_port)->default_value(listen_auto_detect_port),
                                            "Automatically select ports for listening")
        ("listen_port_min",                 po::value<int>(&listen_port_min)->default_value(listen_port_min),
                                            "Select minimal port number for range")
        ("listen_port_max",                 po::value<int>(&listen_port_max)->default_value(listen_port_max),
                                            "Select maximum port number for range")

        ("magnet_resolve_timeout",          po::value<int>(&magnet_resolve_timeout)->default_value(magnet_resolve_timeout),
                                            "Timeout for resolving magnet (in seconds)")

        ("tuned_storage",                   po::value<bool>(&tuned_storage)->default_value(tuned_storage),
                                            "Use tuned File storage (to lower random IO writes)")
        ("disk_cache_size",                 po::value<int>(&disk_cache_size)->default_value(disk_cache_size),
                                            "Cache size use for File storage (in MB)")

        ("session_save",                    po::value<int>(&session_save)->default_value(session_save),
                                            "Seconds between session backup")
        ("encryption_policy",               po::value<std::string>(&encryption_policy_arg)->default_value(encryption_policy_arg),
                                            "Encryption policy")
        ("spoof_user_agent",                po::value<std::string>(&spoof_user_agent_arg)->default_value(spoof_user_agent_arg),
                                            "Custom user agent for libtorrent")

        ("proxy_enabled",                   po::value<bool>(&proxy_enabled)->default_value(proxy_enabled),
                                            "Enable proxy")
        ("proxy_type",                      po::value<std::string>(&proxy_type_arg)->default_value(proxy_type_arg),
                                            "Proxy type")
        ("proxy_host",                      po::value<std::string>(&proxy_host)->default_value(proxy_host),
                                            "Proxy host")
        ("proxy_port",                      po::value<int>(&proxy_port)->default_value(proxy_port),
                                            "Proxy port")
        ("proxy_login",                     po::value<std::string>(&proxy_login)->default_value(proxy_login),
                                            "Proxy login")
        ("proxy_password",                  po::value<std::string>(&proxy_password)->default_value(proxy_password),
                                            "Proxy password")
        ("use_proxy_tracker",               po::value<bool>(&use_proxy_tracker)->default_value(use_proxy_tracker),
                                            "Use proxy for requesting trackers")
        ("use_proxy_download",              po::value<bool>(&use_proxy_download)->default_value(use_proxy_download),
                                            "Use proxy for downloading data")

        ("use_libtorrent_logging",          po::value<bool>(&use_libtorrent_logging)->default_value(use_libtorrent_logging),
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

    // Read configuration from file
    if (vm.count("config") && !vm["config"].defaulted()) {
        std::ifstream ifs(config_file.c_str());
        if (!ifs) {
            OATPP_LOGE("Config", "Cannot open config file: %s", config_file.c_str());
            throw std::invalid_argument("Cannot open configuration file:" + config_file);
        }

        OATPP_LOGI("Config", "Reading configuration from file: %s", config_file.c_str());

        std::string json_data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        JS::ParseContext context(json_data);

        context.parseTo(*this);
    }

    // Convert string variables into enum types
    if (vm.count("download_storage") && !vm["download_storage"].defaulted()) {
        download_storage = lh::from_string<lh::storage_type_t, lh::js_storage_type_t_string_struct>(download_storage_arg);
    }
    if (vm.count("listen_interfaces") && !vm["listen_interfaces"].defaulted()) {
        boost::algorithm::split(listen_interfaces, listen_interfaces_arg, boost::is_any_of(", "), boost::token_compress_on);
    }
    if (vm.count("outgoing_interfaces") && !vm["outgoing_interfaces"].defaulted()) {
        boost::algorithm::split(outgoing_interfaces, outgoing_interfaces_arg, boost::is_any_of(", "), boost::token_compress_on);
    }
    if (vm.count("auto_memory_size_strategy") && !vm["auto_memory_size_strategy"].defaulted()) {
        auto_memory_size_strategy =
            lh::from_string<lh::auto_memory_strategy_type_t, lh::js_auto_memory_strategy_type_t_string_struct>(
                auto_memory_size_strategy_arg);
    }
    if (vm.count("encryption_policy") && !vm["encryption_policy"].defaulted()) {
        encryption_policy = lh::from_string<lh::encryption_type_t, lh::js_encryption_type_t_string_struct>(encryption_policy_arg);
    }
    if (vm.count("spoof_user_agent") && !vm["spoof_user_agent"].defaulted()) {
        spoof_user_agent = lh::from_string<lh::user_agent_type_t, lh::js_user_agent_type_t_string_struct>(spoof_user_agent_arg);
    }
    if (vm.count("proxy_type") && !vm["proxy_type"].defaulted()) {
        proxy_type = lh::from_string<lh::proxy_type_t, lh::js_proxy_type_t_string_struct>(proxy_type_arg);
    }

    // Write current configuration to desired file
    if (vm.count("write") && !vm["write"].defaulted()) {
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

    // Autoselect memory size
    if (auto_memory_size) {
        if (auto_memory_size_strategy == auto_memory_strategy_type_t::min) {
            memory_size = memory_size_min / 1024 / 1024;
        } else {
            // We try to use 15 percent of physical memory size for MAX strategy and 8 percent for usual strategy.
            int pct = auto_memory_size_strategy == auto_memory_strategy_type_t::max ? 15 : 8;

            auto total = get_total_system_memory();
            std::int64_t mem = total / 100 * pct;
            if (mem > 0) {
                memory_size = int(mem / 1024 / 1024);
            }

            OATPP_LOGI("Config", "Total system memory: %s", humanize_bytes(total).c_str());
            OATPP_LOGI("Config", "Automatically selected memory size: %s", humanize_bytes(memory_size*1024*1024).c_str());

            if (memory_size*1024*1024 > memory_size_max) {
                OATPP_LOGI("Config", "Selected memory size (%s) is bigger than maximum for auto-select (%s), so we decrease memory size to maximum allowed: %s", 
                    humanize_bytes(memory_size*1024*1024).c_str(), humanize_bytes(memory_size_max).c_str(), humanize_bytes(memory_size_max).c_str());
                memory_size = memory_size_max / 1024/1024;
            }
        }
    }
}
}