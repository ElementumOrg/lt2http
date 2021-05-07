#include "session.h"
#include "app/application.h"

#include <libtorrent/alert_types.hpp>
#include <libtorrent/bdecode.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/torrent_info.hpp>

#include <boost/algorithm/string.hpp>

#include <oatpp/core/Types.hpp>
#include <oatpp/core/base/Environment.hpp>

#include <chrono>
#include <stdexcept>
#include <thread>

#include <bittorrent/reader.h>

#include <utils/exceptions.h>
#include <utils/numbers.h>
#include <utils/path.h>
#include <utils/strings.h>

using clk = std::chrono::steady_clock;

namespace lh {

Session::Session(lh::Config &config) : m_config(config) {
    OATPP_LOGI("Session", "Starting libtorrent session");

    // Configure lt::settings_pack
    configure();

    // Start lt::session
    lt::session_params params{m_pack};
    m_nativeSession = std::make_shared<lt::session>(params, lt::session::add_default_plugins);

    // Start lt:: services
    start_services();

    // Load previous torrents
    load_previous_torrents();

    // Start Alerts watcher in a separate thread
    std::thread alert_thread(&Session::consume_alerts, this);
    std::thread prioritize_thread(&Session::prioritize, this);
    alert_thread.detach();
    prioritize_thread.detach();
}

Session::~Session() = default;

std::shared_ptr<Config> Session::config() { return std::shared_ptr<Config>(&m_config); }

void Session::configure() {
    auto user_agent = get_user_agent();
    OATPP_LOGI("Session::configure", "UserAgent: %s", user_agent.c_str())

    m_pack.set_str(lt::settings_pack::user_agent, user_agent);

    // Bools
    m_pack.set_bool(lt::settings_pack::announce_to_all_tiers, true);
    m_pack.set_bool(lt::settings_pack::announce_to_all_trackers, true);
    m_pack.set_bool(lt::settings_pack::apply_ip_filter_to_trackers, false);
    // m_pack.set_bool(lt::settings_pack::lazy_bitfields, true);
    m_pack.set_bool(lt::settings_pack::no_atime_storage, true);
    m_pack.set_bool(lt::settings_pack::no_connect_privileged_ports, false);
    m_pack.set_bool(lt::settings_pack::prioritize_partial_pieces, false);
    m_pack.set_bool(lt::settings_pack::rate_limit_ip_overhead, false);
    m_pack.set_bool(lt::settings_pack::smooth_connects, false);
    m_pack.set_bool(lt::settings_pack::strict_end_game_mode, true);
    m_pack.set_bool(lt::settings_pack::upnp_ignore_nonrouters, true);
    m_pack.set_bool(lt::settings_pack::use_dht_as_fallback, false);
    m_pack.set_bool(lt::settings_pack::use_parole_mode, true);
    m_pack.set_bool(lt::settings_pack::piece_priority_order, true);

    // Disabling services, as they are enabled by default in libtorrent
    m_pack.set_bool(lt::settings_pack::enable_upnp, false);
    m_pack.set_bool(lt::settings_pack::enable_natpmp, false);
    m_pack.set_bool(lt::settings_pack::enable_lsd, false);
    m_pack.set_bool(lt::settings_pack::enable_dht, false);

    m_pack.set_int(lt::settings_pack::listen_queue_size, 30);
    m_pack.set_int(lt::settings_pack::alert_queue_size, std::numeric_limits<int>::max() / 2);
    m_pack.set_int(lt::settings_pack::aio_threads, std::max(1, int(std::thread::hardware_concurrency())) * 4);
    m_pack.set_int(lt::settings_pack::cache_size, -1);
    m_pack.set_int(lt::settings_pack::mixed_mode_algorithm, lt::settings_pack::prefer_tcp);

    // Intervals and Timeouts
    m_pack.set_int(lt::settings_pack::auto_scrape_interval, 1200);
    m_pack.set_int(lt::settings_pack::auto_scrape_min_interval, 900);
    m_pack.set_int(lt::settings_pack::min_announce_interval, 30);
    m_pack.set_int(lt::settings_pack::dht_announce_interval, 60);
    m_pack.set_int(lt::settings_pack::stop_tracker_timeout, 1);

    // Ratios
    m_pack.set_int(lt::settings_pack::seed_time_limit, 0);
    m_pack.set_int(lt::settings_pack::seed_time_ratio_limit, 0);
    m_pack.set_int(lt::settings_pack::share_ratio_limit, 0);

    // Algorithms
    m_pack.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::fixed_slots_choker);
    m_pack.set_int(lt::settings_pack::seed_choking_algorithm, lt::settings_pack::fastest_upload);

    // Sizes
    m_pack.set_int(lt::settings_pack::request_queue_time, 2);
    m_pack.set_int(lt::settings_pack::max_out_request_queue, 5000);
    m_pack.set_int(lt::settings_pack::max_allowed_in_request_queue, 5000);
    m_pack.set_int(lt::settings_pack::max_peerlist_size, 50000);
    m_pack.set_int(lt::settings_pack::dht_upload_rate_limit, 50000);
    m_pack.set_int(lt::settings_pack::max_pex_peers, 200);
    m_pack.set_int(lt::settings_pack::max_suggest_pieces, 50);
    m_pack.set_int(lt::settings_pack::whole_pieces_threshold, 10);

    m_pack.set_int(lt::settings_pack::send_buffer_low_watermark, 10 * 1024);
    m_pack.set_int(lt::settings_pack::send_buffer_watermark, 500 * 1024);
    m_pack.set_int(lt::settings_pack::send_buffer_watermark_factor, 50);

    m_pack.set_int(lt::settings_pack::download_rate_limit, 0);
    m_pack.set_int(lt::settings_pack::upload_rate_limit, 0);

    // For Android external storage / OS-mounted NAS setups
    if (m_config.tuned_storage && !is_memory_storage()) {
        m_pack.set_bool(lt::settings_pack::use_read_cache, true);
        m_pack.set_bool(lt::settings_pack::coalesce_reads, true);
        m_pack.set_bool(lt::settings_pack::coalesce_writes, true);
        m_pack.set_int(lt::settings_pack::max_queued_disk_bytes, m_config.disk_cache_size * 1024 * 1024);
    }

    if (m_config.connections_limit > 0) {
        m_pack.set_int(lt::settings_pack::connections_limit, m_config.connections_limit);
    }

    if (m_config.conntracker_limit_auto || m_config.conntracker_limit == 0) {
        m_pack.set_int(lt::settings_pack::connection_speed, 300);
    } else {
        m_pack.set_int(lt::settings_pack::connection_speed, m_config.conntracker_limit);
    }

    if (!m_config.limit_after_buffering) {
        if (m_config.max_download_rate > 0) {
            OATPP_LOGI("Session::configure", "Rate limiting download to: %d", m_config.max_download_rate * 1024)
            m_pack.set_int(lt::settings_pack::download_rate_limit, m_config.max_download_rate * 1024);
        }
        if (m_config.max_upload_rate > 0) {
            OATPP_LOGI("Session::configure", "Rate limiting upload to: %d", m_config.max_upload_rate * 1024)
            m_pack.set_int(lt::settings_pack::upload_rate_limit, m_config.max_upload_rate * 1024);
        }
    }

    // TODO: Enable when it's working!
    // if s.config.DisableUpload {
    // 	s.Session.AddUploadExtension()
    // }

    if (!m_config.seed_forever) {
        if (m_config.share_ratio_limit > 0) {
            m_pack.set_int(lt::settings_pack::share_ratio_limit, m_config.share_ratio_limit);
        }
        if (m_config.seed_time_ratio_limit > 0) {
            m_pack.set_int(lt::settings_pack::seed_time_ratio_limit, m_config.seed_time_ratio_limit);
        }
        if (m_config.seed_time_limit > 0) {
            m_pack.set_int(lt::settings_pack::seed_time_limit, m_config.seed_time_limit * 3600);
        }
    }

    OATPP_LOGI("Session::configure", "Applying encryption settings...")
    m_pack.set_int(lt::settings_pack::allowed_enc_level, lt::settings_pack::pe_rc4);
    m_pack.set_bool(lt::settings_pack::prefer_rc4, true);

    switch (m_config.encryption_policy) {
    case encryption_type_t::enabled: // Enabled
        m_pack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_enabled);
        m_pack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_enabled);
        break;
    case lh::encryption_type_t::forced: // Forced
        m_pack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
        m_pack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
        break;
    default: // Disabled
        m_pack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_disabled);
        m_pack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_disabled);
    }

    m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::none);
    if (m_config.proxy_enabled && !m_config.proxy_host.empty()) {
        OATPP_LOGI("Session::configure", "Applying proxy settings...")
        if (m_config.proxy_type == lh::proxy_type_t::socks4) {
            m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks4);
        } else if (m_config.proxy_type == lh::proxy_type_t::socks5) {
            m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks5);
        } else if (m_config.proxy_type == lh::proxy_type_t::socks5_pw) {
            m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks5_pw);
        } else if (m_config.proxy_type == lh::proxy_type_t::http) {
            m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::http);
        } else if (m_config.proxy_type == lh::proxy_type_t::http_pw) {
            m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::http_pw);
        } else if (m_config.proxy_type == lh::proxy_type_t::i2p_proxy) {
            m_pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::i2p_proxy);
            m_pack.set_int(lt::settings_pack::i2p_port, m_config.proxy_port);
            m_pack.set_str(lt::settings_pack::i2p_hostname, m_config.proxy_host);
            m_pack.set_bool(lt::settings_pack::allow_i2p_mixed, true);
        }

        m_pack.set_int(lt::settings_pack::proxy_port, m_config.proxy_port);
        m_pack.set_str(lt::settings_pack::proxy_hostname, m_config.proxy_host);
        m_pack.set_str(lt::settings_pack::proxy_username, m_config.proxy_login);
        m_pack.set_str(lt::settings_pack::proxy_password, m_config.proxy_password);

        // Proxy files downloads
        m_pack.set_bool(lt::settings_pack::proxy_peer_connections, m_config.use_proxy_download);
        m_pack.set_bool(lt::settings_pack::proxy_hostnames, m_config.use_proxy_download);

        // Proxy Tracker connections
        m_pack.set_bool(lt::settings_pack::proxy_tracker_connections, m_config.use_proxy_tracker);
    }

    // Set alert_mask here so it also applies on reconfigure...
    m_pack.set_int(lt::settings_pack::alert_mask, lt::alert::error_notification | lt::alert::file_progress_notification |
                                                      lt::alert::performance_warning | lt::alert::status_notification |
                                                      lt::alert::tracker_notification | lt::alert_category::status);

    if (m_config.use_libtorrent_logging) {
        m_pack.set_int(lt::settings_pack::alert_mask, lt::alert::all_categories);
    }

    OATPP_LOGI("Session::configure", "DownloadStorage: %s", storage_to_string(m_config.download_storage))
    if (is_memory_storage()) {
        int need_size = m_config.buffer_size * 1024 * 1024 + int(m_config.end_buffer_size) * 1024 * 1024 + 8 * 1024 * 1024;
        int msize = memory_size();

        if (msize < need_size) {
            OATPP_LOGI("Session::configure", "Raising memory size (from %d) to fit all the buffer (to %d)",
                       humanize_bytes(msize).c_str(), humanize_bytes(need_size).c_str())
            msize = need_size;
            m_config.memory_size = msize / 1024 / 1024;
        }

        OATPP_LOGI("Session::configure", "Using memory size: %s", humanize_bytes(msize).c_str())

        // Set Memory storage specific settings
        m_pack.set_bool(lt::settings_pack::close_redundant_connections, false);

        m_pack.set_int(lt::settings_pack::share_ratio_limit, 0);
        m_pack.set_int(lt::settings_pack::seed_time_ratio_limit, 0);
        m_pack.set_int(lt::settings_pack::seed_time_limit, 0);

        m_pack.set_int(lt::settings_pack::active_downloads, -1);
        m_pack.set_int(lt::settings_pack::active_seeds, -1);
        m_pack.set_int(lt::settings_pack::active_limit, -1);
        m_pack.set_int(lt::settings_pack::active_tracker_limit, -1);
        m_pack.set_int(lt::settings_pack::active_dht_limit, -1);
        m_pack.set_int(lt::settings_pack::active_lsd_limit, -1);
        // m_pack.set_int(lt::settings_pack::read_cache_line_size, 0);
        // m_pack.set_int(lt::settings_pack::unchoke_slots_limit, 0);

        // m_pack.set_int(lt::settings_pack::request_timeout, 10);
        // m_pack.set_int(lt::settings_pack::peer_connect_timeout, 10);

        m_pack.set_int(lt::settings_pack::max_out_request_queue, 100000);
        m_pack.set_int(lt::settings_pack::max_allowed_in_request_queue, 100000);

        // m_pack.set_int("initial_picker_threshold", 20);
        // m_pack.set_int("share_mode_target", 1);
        m_pack.set_bool(lt::settings_pack::use_read_cache, false);
        m_pack.set_bool(lt::settings_pack::auto_sequential, false);

        // m_pack.set_int("tick_interval", 300);
        // m_pack.set_bool("strict_end_game_mode", false);

        // m_pack.set_int("disk_io_write_mode", 2);
        // m_pack.set_int("disk_io_read_mode", 2);
        m_pack.set_int(lt::settings_pack::cache_size, 0);

        // Adjust timeouts to avoid disconnect due to idling connections
        m_pack.set_int(lt::settings_pack::inactivity_timeout, 60 * 30);
        m_pack.set_int(lt::settings_pack::peer_timeout, 60 * 10);
        m_pack.set_int(lt::settings_pack::min_reconnect_time, 20);
    }

    if (m_config.listen_auto_detect_port) {
        m_config.listen_port_min = 6891;
        m_config.listen_port_max = 6899;
    }

    std::vector<std::string> listen_interfaces{"0.0.0.0", "[::]"};

    if (!m_config.listen_auto_detect_ip && !m_config.listen_interfaces.empty()) {
        listen_interfaces.clear();
        listen_interfaces.assign(m_config.listen_interfaces.begin(), m_config.listen_interfaces.end());
    }

    m_mapped_ports.clear();
    std::string listen_interfaces_result;
    for (const auto &i : listen_interfaces) {
        int port = random(m_config.listen_port_min, m_config.listen_port_max);
        m_mapped_ports[port] = {};
        if (!listen_interfaces_result.empty())
            listen_interfaces_result += ",";

        listen_interfaces_result += i + ":" + std::to_string(port);

        if (m_config.listen_port_max - m_config.listen_port_min > 0) {
            port = random(m_config.listen_port_min, m_config.listen_port_max);
            m_mapped_ports[port] = {};
            if (!listen_interfaces_result.empty())
                listen_interfaces_result += ",";

            listen_interfaces_result += i + ":" + std::to_string(port);
        }
    }

    m_pack.set_str(lt::settings_pack::listen_interfaces, listen_interfaces_result);
    OATPP_LOGI("Session::configure", "Listening on: %s", listen_interfaces_result.c_str());

    if (!m_config.outgoing_interfaces.empty()) {
        m_pack.set_str(lt::settings_pack::outgoing_interfaces, join(m_config.outgoing_interfaces, ","));
    }
}

void Session::reconfigure() {
    OATPP_LOGI("Session::reconfigure", "Reconfiguring libtorrent session");
    m_config = lh::config();
    configure();
}

void Session::start_services() {
    if (!m_config.disable_lsd) {
        OATPP_LOGI("Session::start_services", "Starting LSD...");
        m_pack.set_bool(lt::settings_pack::enable_lsd, true);
    }

    if (!m_config.disable_dht) {
        OATPP_LOGI("Session::start_services", "Starting DHT...");
        m_pack.set_str(lt::settings_pack::dht_bootstrap_nodes,
                       "dht.libtorrent.org:25401,router.bittorrent.com:6881,router.utorrent.com:6881,dht.transmissionbt.com:6881,"
                       "dht.aelitis.com:6881");
        m_pack.set_bool(lt::settings_pack::enable_dht, true);
    }

    if (!m_config.disable_upnp) {
        OATPP_LOGI("Session::start_services", "Starting UPNP...");
        m_pack.set_bool(lt::settings_pack::enable_upnp, true);

        OATPP_LOGI("Session::start_services", "Starting NATPMP...");
        m_pack.set_bool(lt::settings_pack::enable_natpmp, true);
    }

    m_nativeSession->apply_settings(m_pack);

    for (const auto &p : m_mapped_ports) {
        OATPP_LOGI("Session::start_services", "Adding port mapping: %d", p.first);
        m_mapped_ports[p.first] = m_nativeSession->add_port_mapping(lt::session::tcp, p.first, p.first);
    }
}

void Session::stop_services() {
    if (!m_config.disable_lsd) {
        OATPP_LOGI("Session::stop_services", "Stopping LSD...");
        m_pack.set_bool(lt::settings_pack::enable_lsd, false);
    }

    if (!m_config.disable_dht) {
        OATPP_LOGI("Session::stop_services", "Stopping DHT...");
        m_pack.set_bool(lt::settings_pack::enable_dht, false);
    }

    if (!m_config.disable_upnp) {
        OATPP_LOGI("Session::stop_services", "Stopping UPNP...");
        m_pack.set_bool(lt::settings_pack::enable_upnp, false);

        OATPP_LOGI("Session::stop_services", "Stopping NATPMP...");
        m_pack.set_bool(lt::settings_pack::enable_natpmp, false);
    }

    for (const auto &p : m_mapped_ports) {
        OATPP_LOGI("Session::stop_services", "Deleting port mapping for post: %d", p.first);
        for (auto i : p.second) {
            m_nativeSession->delete_port_mapping(i);
        }
    }
    m_mapped_ports.clear();

    m_nativeSession->apply_settings(m_pack);
}

void Session::consume_alerts() {
    OATPP_LOGI("Session::consume_alerts", "Starting alerts consumer thread");

    clk::time_point last_save_resume = clk::now();
    std::chrono::seconds save_resume_period{30};

    for (;;) {
        std::vector<lt::alert *> alerts;
        m_nativeSession->pop_alerts(&alerts);

        for (const lt::alert *a : alerts) {
            if (lh::is_closing) {
                OATPP_LOGI("Session::consume_alerts", "Stop alert processing due to closing application");
                return;
            }

            switch (a->type()) {
            case lt::session_stats_header_alert::alert_type:
            case lt::file_completed_alert::alert_type:
                // Ignore alert
                break;
            case lt::save_resume_data_alert::alert_type:
            case lt::dht_reply_alert::alert_type:
            case lt::tracker_announce_alert::alert_type:
            case lt::tracker_error_alert::alert_type:
            case lt::tracker_reply_alert::alert_type:
            case lt::tracker_warning_alert::alert_type:
                // Handle alert to specific torrent
                dispatch_alert(a);
                break;
            case lt::state_update_alert::alert_type:
                handle_state_update_alert(static_cast<const lt::state_update_alert *>(a));
                break;
            case lt::session_stats_alert::alert_type:
                handle_session_stats_alert(static_cast<const lt::session_stats_alert *>(a));
                break;
            case lt::metadata_received_alert::alert_type:
                handle_metadata_received_alert(static_cast<const lt::metadata_received_alert *>(a));
                break;
            default:
                OATPP_LOGI("Session::consume_alerts", "Alert: %s, Message: %s", a->what(), a->message().c_str());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (lh::is_closing) {
            OATPP_LOGI("Session::consume_alerts", "Stop alert processing due to closing application");
            return;
        }

        if (clk::now() - last_save_resume > save_resume_period) {
            last_save_resume = clk::now();
            trigger_resume_data();
        }

        if (m_refresh_requested)
            m_refresh_requested = false;
        else
            request_update();
    }
}

void Session::prioritize() {
    OATPP_LOGI("Session::prioritize", "Starting prioritizer thread");

    for (;;) {
        for (const auto &t : m_torrents) {
            if (t->is_buffering()) {
                // Calculate buffer progress
                t->update_buffer_progress();

                continue;
            }

            if (!t->has_readers())
                continue;

            t->prioritize();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (lh::is_closing) {
            OATPP_LOGI("Session::consume_alerts", "Stop alert processing due to closing application");
            return;
        }
    }
}

void Session::request_update() {
    m_nativeSession->post_torrent_updates();
    m_nativeSession->post_session_stats();

    m_refresh_requested = true;
}

void Session::close() {
    OATPP_LOGI("Session::close", "Stopping libtorrent session");

    m_isClosing = true;

    // Stop lt:: services
    stop_services();

    for (const auto &t : m_torrents)
        t->files().clear();
}

void Session::load_previous_torrents() {
    if (!m_config.autoload_torrents || m_config.torrents_path.empty())
        return;

    m_torrents.clear();

    OATPP_LOGI("Session::load_previous_torrents", "Loading previous torrents from: %s", m_config.torrents_path.c_str());
    lt::error_code ec;
    std::vector<std::string> entries = list_dir(
        m_config.torrents_path, [](lt::string_view p) { return p.size() > 8 && p.substr(p.size() - 8) == ".torrent"; }, ec);
    if (ec) {
        OATPP_LOGE("Session::load_previous_torrents", "Failed to list directory '%s': %s", m_config.torrents_path.c_str(),
                   ec.message().c_str());
        return;
    }

    for (auto &p : entries) {
        std::string path = path_append(m_config.torrents_path, p);
        try {
            add_torrent(path, m_config.autoload_torrents_paused, lh::storage_type_t::file);
        } catch (std::exception &e) {
            OATPP_LOGE("Session::load_previous_torrents", "Failed to load torrent '%s': %s", path.c_str(), ec.message().c_str());
        }
    }
}

std::shared_ptr<Torrent> Session::add_torrent(std::string &uri, bool is_paused, lh::storage_type_t st) {
    // Trim whitespaces
    boost::trim(uri);
    if (uri.empty())
        throw lh::Exception("Empty uri parameter");

    std::string storage_description = storage_to_string(st);
    if (st == storage_type_t::automatic) {
        st = m_config.download_storage;
        storage_description += " (" + std::string(storage_to_string(st)) + ")";
    }

    OATPP_LOGI("Session::add_torrent", "Adding torrent with uri: %s, storage: %s", uri.c_str(), storage_description.c_str())

    lt::error_code ec;
    lt::add_torrent_params p;

    bool is_memory_storage = st == storage_type_t::memory;
    bool is_skip_priorities = false;
    std::string hash;

    // Check if download directory is here
    if (!is_memory_storage && m_config.download_path.empty()) {
        OATPP_LOGI("Session::add_torrent", "Skipping adding torrent due to missing download path")
        throw lh::Exception("Missing download path for using file storage");
    }

    // Check if file actually contains a magnet link
    if (uri.rfind("magnet", 0) != 0) {
        std::string file_body;

        if (load_file(uri, file_body) && file_body.rfind("magnet", 0) == 0) {
            remove_temporary_file(uri);

            uri = file_body;
        }
    }

    // Read torrent properties
    if (uri.rfind("magnet", 0) == 0) {
        // Magnet link
        lt::parse_magnet_uri(uri, p, ec);
        if (ec) {
            OATPP_LOGI("Session::add_torrent", "Failed to load torrent file with magnet '%s': %s", uri.c_str(),
                       ec.message().c_str());
            throw lh::Exception(ec.message());
        }
        hash = to_hex(p.info_hash);
    } else {
        // Local file
        auto ti = std::make_shared<lt::torrent_info>(uri, ec);

        remove_temporary_file(uri);
        if (ec) {
            OATPP_LOGI("Session::add_torrent", "Failed to load torrent file with path '%s': %s", uri.c_str(),
                       ec.message().c_str());
            throw lh::Exception(ec.message());
        }

        p.ti = ti;
        if (p.ti)
            hash = to_hex(p.ti->info_hash());
    }

    if (hash.empty())
        hash = to_hex(p.info_hash);

    if (is_memory_storage) {
        // Set memory size for memory_storage
        lh::memory_size = memory_size();
        p.storage = lh::memory_storage_constructor;
    } else {
        // Read fastresume files
        std::vector<char> resume_data;
        auto resume_path = resume_file(m_config.torrents_path, hash);
        if (load_file(resume_path, resume_data)) {
            p = lt::read_resume_data(resume_data, ec);
            if (ec) {
                OATPP_LOGI("Session::add_torrent", "Failed to load resume data: %s", ec.message().c_str());
            } else {
                OATPP_LOGI("Session::add_torrent", "Using resume data: %s", resume_path.c_str());
                if (p.ti)
                    hash = to_hex(p.ti->info_hash());

                is_skip_priorities = true;
            }
        }
    }

    if (ec)
        throw lh::Exception(ec.message());
    if (has_torrent(hash))
        throw lh::Exception(Fmt("Torrent with hash '%s' already exists", hash.c_str()));

    p.max_connections = get_max_connections();
    p.save_path = m_config.download_path;

    if (!is_skip_priorities) {
        for (auto i = 0; i <= 1000; i++)
            p.file_priorities.push_back(lt::download_priority_t{0});
    }

    OATPP_LOGI("Session::add_torrent", "Setting sequential download to: %s", !is_memory_storage ? "true" : "false");
    if (!is_memory_storage)
        p.flags |= lt::torrent_flags::sequential_download;
    else
        p.flags &= ~lt::torrent_flags::sequential_download;

    lt::torrent_handle th = m_nativeSession->add_torrent(std::move(p), ec);
    if (ec)
        throw lh::Exception(ec.message());

    if (!is_paused)
        th.resume();

    auto torrent = std::make_shared<Torrent>(m_nativeSession, th, st);
    m_torrents.emplace_back(torrent);

    if (torrent->has_metadata())
        torrent->on_metadata_received();

    OATPP_LOGI("Session::add_torrent", "Added torrent with hash: %s", torrent->hash().c_str());

    return torrent;
}

std::vector<std::shared_ptr<Torrent>> Session::torrents() { return m_torrents; }

std::shared_ptr<Torrent> Session::get_torrent(std::string &hash) {
    auto match =
        std::find_if(m_torrents.begin(), m_torrents.end(), [&hash](const std::shared_ptr<Torrent>& t) { return t->hash() == hash; });
    if (match == m_torrents.end())
        throw lh::TorrentException(Fmt("Torrent with hash '%s' not found", hash.c_str()));

    return *match;
}

bool Session::has_torrent(std::string &hash) {
    auto match = std::find_if(m_torrents.begin(), m_torrents.end(),
                              [&hash](const std::shared_ptr<Torrent> &t) { return t->hash() == hash; });
    return match != m_torrents.end();
}

bool Session::remove_torrent(std::string &hash, bool is_delete_files, bool is_delete_data) {
    auto torrent = get_torrent(hash);
    torrent->remove(is_delete_files, is_delete_data);

    OATPP_LOGI("Session::remove_torrent", "Removed torrent with hash: %s", torrent->hash().c_str());

    auto match = std::find_if(m_torrents.begin(), m_torrents.end(),
                              [&hash](const std::shared_ptr<Torrent> &t) { return t->hash() == hash; });
    m_torrents.erase(match);

    return true;
}

std::string Session::get_user_agent() const {
    switch (m_config.spoof_user_agent) {
    case lh::user_agent_type_t::lt2http:
        return "lt2http/" + lh::VERSION;
    case lh::user_agent_type_t::transmission_1_9_3:
        return "Transmission/1.93";
    case lh::user_agent_type_t::transmission_2_9_2:
        return "Transmission/2.92";
    case lh::user_agent_type_t::libtorrent_1_1_0:
        return "libtorrent (Rasterbar) 1.1.0";
    case lh::user_agent_type_t::libtorrent_1_2_0:
        return "libtorrent (Rasterbar) 1.2.0";
    case lh::user_agent_type_t::bittorrent_7_4_3:
        return "BitTorrent/7.4.3";
    case lh::user_agent_type_t::bittorrent_7_5_0:
        return "BitTorrent/7.5.0";
    case lh::user_agent_type_t::utorrent_2_2_1:
        return "uTorrent/2.2.1";
    case lh::user_agent_type_t::utorrent_3_2_0:
        return "uTorrent/3.2.0";
    case lh::user_agent_type_t::utorrent_3_4_9:
        return "uTorrent/3.4.9";
    case lh::user_agent_type_t::deluge_1_3_6_0:
        return "Deluge/1.3.6.0";
    case lh::user_agent_type_t::deluge_1_3_12_0:
        return "Deluge/1.3.12.0";
    case lh::user_agent_type_t::vuze_5_7_3_0:
        return "Vuze/5.7.3.0";
    default:
        return "lt2http/" + lh::VERSION;
    }
}

int Session::get_max_connections() {
    int cpu = int(std::thread::hardware_concurrency());
    if (cpu <= 1)
        return 30;
    
    return 75;
}

bool Session::is_memory_storage() const { return m_config.download_storage == storage_type_t::memory; }

bool Session::is_closing() const { return m_isClosing; }

void Session::handle_state_update_alert(const lt::state_update_alert *p) {
    if (p->status.empty())
        return;

    for (const lt::torrent_status &status : p->status) {
        auto hash = to_hex(status.info_hash);

        if (!has_torrent(hash))
            continue;

        auto torrent = get_torrent(hash);
        torrent->update_status(status);
    }
}

void Session::handle_metadata_received_alert(const lt::metadata_received_alert *p) {
    std::string hash = to_hex(p->handle.info_hash());
    if (!has_torrent(hash))
        return;

    OATPP_LOGI("Session::handle_metadata_received_alert", "Received metadata for '%s'", hash.c_str());
    auto torrent = get_torrent(hash);
    torrent->update_metadata();
}

void Session::handle_session_stats_alert(const lt::session_stats_alert *p) {
    std::vector<lt::stats_metric> map = lt::session_stats_metrics();
    auto counters = p->counters();

    m_session_stats.clear();
    for (lt::stats_metric const &m : map) {
        m_session_stats[m.name] = counters[m.value_index];
    }
}

void Session::dispatch_alert(const lt::alert *a) {
    auto hash = to_hex(static_cast<const lt::torrent_alert *>(a)->handle.info_hash());
    if (!has_torrent(hash))
        return;

    auto torrent = get_torrent(hash);
    torrent->dispatch_alert(a);
}

void Session::remove_temporary_file(std::string &path) {
    if (!file_exists(path))
        return;

    // Check if file has a temporary prefix to avoid removing non-temporary files.
    if (path.find("/tmp_") == std::string::npos)
        return;

    OATPP_LOGI("Session::remove_temporary_file", "Removing temporary file at: %s", path.c_str());
    std::remove(path.c_str());
}

void Session::trigger_resume_data() {
    for (const auto &torrent : m_torrents) {
        torrent->save_resume_data();
    }
}

std::int64_t Session::memory_size() const { return m_config.memory_size * 1024 * 1024; };

bool Session::is_paused() const {
    return m_nativeSession->is_paused();
};

void Session::pause() {
    m_nativeSession->pause();
};
void Session::resume() {
    m_nativeSession->resume();
};

} // namespace lh
