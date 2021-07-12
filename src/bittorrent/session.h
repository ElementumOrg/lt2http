#pragma once

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_status.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/version.hpp>

#include <app/config.h>

#include <bittorrent/memory_storage.h>
#include <bittorrent/torrent.h>

namespace lh {

class Session {
  private:
    std::shared_ptr<lt::session> m_nativeSession = nullptr;
    lt::settings_pack m_pack;

    std::map<std::string, std::int64_t> m_session_stats;
    std::map<int, std::vector<lt::port_mapping_t>> m_mapped_ports;

    lh::Config m_config;
    std::vector<std::shared_ptr<Torrent>> m_torrents;

    bool m_refresh_requested = false;
    bool m_isClosing = false;

  public:
    explicit Session(lh::Config &config);
    ~Session();

    std::shared_ptr<Config> config();
    void run();
    void configure();
    void reconfigure();
    void start_services();
    void stop_services();
    void consume_alerts();
    void prioritize();
    void request_update();
    void close();
    void load_previous_torrents();

    std::vector<std::shared_ptr<Torrent>> torrents();
    std::shared_ptr<Torrent> add_torrent(std::string &uri, bool is_paused, lh::storage_type_t st);
    std::shared_ptr<Torrent> get_torrent(std::string &hash);
    bool has_torrent(std::string &hash);
    bool remove_torrent(std::string &hash, bool is_delete_files, bool is_delete_data);

    std::string get_user_agent() const;
    static int get_max_connections();
    bool is_memory_storage() const;
    bool is_closing() const;

    void handle_state_update_alert(const lt::state_update_alert *p);
    void handle_metadata_received_alert(const lt::metadata_received_alert *p);
    void handle_session_stats_alert(const lt::session_stats_alert *p);
    void dispatch_alert(const lt::alert *a);

    static void remove_temporary_file(std::string &path);
    void trigger_resume_data();

    std::int64_t memory_size() const;

    bool is_paused() const;
    void pause();
    void resume();
};

} // namespace lh
