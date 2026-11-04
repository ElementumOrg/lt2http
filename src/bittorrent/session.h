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

class Session {
  private:
    std::shared_ptr<lt::session> m_nativeSession = nullptr;
    lt::settings_pack m_pack;

    std::map<std::string, std::int64_t> m_session_stats;
    std::map<int, std::vector<lt::port_mapping_t>> m_mapped_ports;
    std::vector<NatMapping> m_nat_mappings;
    // Guards m_pack, m_config and the port mappings, which the NAT-PMP thread changes too
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

    static void remove_temporary_file(const std::string &path);
    void trigger_resume_data();

    std::int64_t memory_size() const;

    bool is_paused() const;
    void pause();
    void resume();
};

} // namespace lh
