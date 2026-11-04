#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_status.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/version.hpp>

#include <app/config.h>

#include <bittorrent/memory_storage.h>
#include <bittorrent/natpmp.h>
#include <bittorrent/torrent.h>

namespace lh {

// A listen socket of libtorrent, from its listen alerts
struct ListenSocket {
    std::string address;
    int port = 0;
    // tcp, tcp_ssl, udp, i2p, socks5 or utp_ssl
    std::string type;
    // Empty when listening
    std::string error;
};

// A port mapping of libtorrent's UPnP or NAT-PMP, from its port mapping alerts
struct PortMapping {
    // UPnP or NAT-PMP
    std::string transport;
    // 0 when unknown
    int local_port = 0;
    std::string message;
    bool is_error = false;
};

// For /settings/state
struct NetworkState {
    // Addresses of this device, for links to the web server from other devices
    std::vector<std::string> web_addresses;
    int web_port = 0;
    // The listen_interfaces setting of libtorrent
    std::string listen_interfaces;
    std::vector<ListenSocket> sockets;
    // lt2http's own NAT-PMP, see natpmp.h
    bool natpmp_enabled = false;
    std::vector<NatMapping> nat_mappings;
    std::vector<PortMapping> port_mappings;
};

class Session {
  private:
    std::shared_ptr<lt::session> m_nativeSession = nullptr;
    lt::settings_pack m_pack;

    // Written by the alerts thread
    std::map<std::string, std::int64_t> m_session_stats;
    mutable std::mutex m_statsMutex;
    std::map<int, std::vector<lt::port_mapping_t>> m_mapped_ports;
    std::vector<NatMapping> m_nat_mappings;
    // Results of the listen and port mapping alerts, by socket and by mapping
    std::map<std::string, ListenSocket> m_listen_sockets;
    std::map<std::string, PortMapping> m_port_mappings;
    // Guards m_pack, m_config, the port mappings and listen sockets, which the NAT-PMP and alerts threads change too
    std::mutex m_portsMutex;
    std::atomic<bool> m_natpmp_rescan{false};

    lh::Config m_config;
    std::vector<std::shared_ptr<Torrent>> m_torrents;
    mutable std::mutex m_torrentsMutex;

    // Written by the loader thread, read when adding torrents.
    std::vector<std::string> m_extraTrackers;
    std::mutex m_extraTrackersMutex;

    bool m_refresh_requested = false;

    // Background threads are owned and joined by the session, so they never outlive it.
    std::vector<std::thread> m_threads;
    std::atomic<bool> m_isClosing{false};
    std::mutex m_cvMutex;
    std::condition_variable m_cv;

    // Waits for the timeout, returns true if the session is closing.
    bool wait_for_abort(std::chrono::milliseconds timeout);
    void start_thread(const char *name, std::function<void()> fn);
    void stop_threads();
    void save_resume_data_on_close();

    std::vector<std::string> listen_interfaces() const;
    std::string listen_endpoints();
    bool natpmp_enabled() const;
    void natpmp();
    void apply_nat_mappings(std::vector<NatMapping> mappings);
    void add_port_mappings();
    void delete_port_mappings();

  public:
    explicit Session(lh::Config &config);
    ~Session();

    void run();
    void check_directories();
    void configure();
    void reconfigure();
    void start_services();
    void stop_services();
    void consume_alerts();
    void prioritize();
    void request_update();
    void update_rate_limits(bool is_buffering);
    void close();
    void load_previous_torrents();
    void load_trackers();

    std::vector<std::shared_ptr<Torrent>> torrents();
    std::shared_ptr<Torrent> add_torrent(std::string uri, bool is_paused, lh::storage_type_t st, bool is_first_time,
                                         std::chrono::time_point<std::chrono::system_clock> added_time);
    std::shared_ptr<Torrent> get_torrent(const std::string &hash) const;
    // Returns nullptr if the torrent is not found.
    std::shared_ptr<Torrent> find_torrent(const std::string &hash) const;
    bool has_torrent(const std::string &hash) const;
    bool remove_torrent(const std::string &hash, bool is_delete_files, bool is_delete_data);

    std::string get_user_agent() const;
    static int get_max_connections();
    bool is_memory_storage() const;
    bool is_closing() const;

    void handle_state_update_alert(const lt::state_update_alert *p);
    void handle_metadata_received_alert(const lt::metadata_received_alert *p);
    void handle_session_stats_alert(const lt::session_stats_alert *p);
    void dispatch_alert(const lt::alert *a);
    void handle_alert(const lt::alert *a);
    void handle_network_alert(const lt::alert *a);

    static void remove_temporary_file(const std::string &path);
    void trigger_resume_data();

    std::int64_t memory_size() const;

    // For /settings/state
    lt::settings_pack native_settings() const;
    std::map<std::string, std::int64_t> session_stats() const;
    int listen_port() const;
    bool is_dht_running() const;
    NetworkState network_state();

    bool is_paused() const;
    void pause();
    void resume();
};

} // namespace lh
