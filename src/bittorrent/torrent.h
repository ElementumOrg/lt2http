#pragma once

#include <atomic>
#include <chrono>
#include <memory>

#include <libtorrent/alert_types.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/download_priority.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include <app/config.h>
#include <bittorrent/memory_storage.h>

namespace lh {

class Reader;
class File;

enum class TorrentState {
    Unknown = -1,
    Queued,

    ForcedDownloading,
    Downloading,
    DownloadingMetadata,
    StalledDownloading,

    ForcedUploading,
    Uploading,
    StalledUploading,

    CheckingResumeData,
    QueuedDownloading,
    QueuedUploading,

    CheckingUploading,
    CheckingDownloading,

    PausedDownloading,
    PausedUploading,

    Moving,

    MissingFiles,
    Error
};

struct TrackerInfo {
    std::string lastMessage;
    int numPeers = 0;
};

class Session;

class Torrent : public std::enable_shared_from_this<lh::Torrent> {
  private:
    std::shared_ptr<lt::session> m_nativeSession;
    lt::torrent_handle m_nativeHandle;
    lt::torrent_status m_nativeStatus;
    std::shared_ptr<lt::torrent_info> m_nativeInfo;
    lt::storage_interface* m_file_storage = nullptr;
    lh::memory_storage* m_memory_storage = nullptr;

    lh::storage_type_t m_storageType;
    TorrentState m_state = TorrentState::Unknown;

    std::vector<std::shared_ptr<File>> m_files;
    std::map<std::int64_t, Reader*> m_readers;
    std::map<std::string, TrackerInfo> m_trackerInfos;

    std::string m_name;
    std::string m_hash;

    std::string m_torrentFile;
    std::string m_resumeFile;
    std::string m_partsFile;

    bool m_hasSeedStatus = false;
    bool m_isStopped = false;

    std::chrono::time_point<std::chrono::system_clock> m_addedTime;
    bool m_isClosing = false;

  public:
    Torrent(std::shared_ptr<lt::session> nativeSession, const lt::torrent_handle &nativeHandle, lh::storage_type_t st);
    ~Torrent();

    std::shared_ptr<Torrent> get_shared_ptr() {
        return shared_from_this();
    }

    lt::torrent_status &status();
    std::vector<std::shared_ptr<File>> files();
    std::shared_ptr<File> get_file(int index);
    lt::storage_interface* storage();

    bool is_closing() const;
    bool is_valid() const;
    std::string hash() const;
    std::string name() const;
    std::int64_t size() const;
    int files_count() const;
    int piece_length() const;
    int piece_length_at(int index) const;
    int pieces_count() const;
    bool have_piece(lt::piece_index_t piece) const;

    bool has_metadata() const;
    void update_metadata();
    void on_metadata_received();
    void remove(bool is_delete_files, bool is_delete_data);
    bool is_memory_storage() const;

    void update_status();
    void update_status(const lt::torrent_status &nativeStatus);
    TorrentState state() const;
    std::string state_string() const;
    void update_state();
    bool has_error() const;

    bool is_buffering() const;
    bool is_paused() const;
    bool is_queued() const;
    bool is_checking() const;
    bool is_downloading() const;
    bool is_uploading() const;
    bool is_completed() const;
    bool is_active() const;
    bool is_inactive() const;
    bool is_errored() const;
    bool is_seed() const;
    bool is_forced() const;
    bool is_sequential_download() const;

    std::int64_t upload_rate() const;
    std::int64_t download_rate() const;
    std::int64_t total() const;
    std::int64_t total_done() const;
    std::int64_t total_wanted() const;
    std::int64_t total_wanted_done() const;
    std::int64_t total_payload_upload() const;
    std::int64_t total_payload_download() const;
    std::int64_t total_download() const;
    std::int64_t total_upload() const;

    std::int64_t active_time() const;
    std::int64_t finished_time() const;
    std::int64_t seeding_time() const;

    int connections_count() const;
    int connections_limit() const;
    int seeds_count() const;
    int peers_count() const;
    int leechers_count() const;
    int total_seeds_count() const;
    int total_peers_count() const;
    int total_leechers_count() const;
    int complete_count() const;
    int incomplete_count() const;

    double progress() const;
    double progress_total() const;
    double progress_wanted() const;
    std::vector<double> files_progress() const;
    std::vector<std::int64_t> files_completed_bytes() const;

    lt::download_priority_t file_priority(lt::file_index_t index) const;
    void file_priority(lt::file_index_t index, lt::download_priority_t priority);
    lt::download_priority_t piece_priority(lt::piece_index_t index) const;
    void piece_priority(lt::piece_index_t index, lt::download_priority_t priority);
    void set_piece_deadline(lt::piece_index_t index, int deadline);
    void prioritize_pieces(lt::piece_index_t start, lt::piece_index_t end);
    void set_piece_priority(int piece, int deadline, lt::download_priority_t priority);

    void dispatch_alert(const lt::alert *a);
    void handle_save_resume_data_alert(const lt::save_resume_data_alert *p);
    void handle_dht_reply_alert(const lt::dht_reply_alert *p);
    void handle_tracker_reply_alert(const lt::tracker_reply_alert *p);
    void handle_tracker_warning_alert(const lt::tracker_warning_alert *p);
    void handle_tracker_error_alert(const lt::tracker_error_alert *p);
    void update_tracker(const std::string &name, std::string message, int number);
    void set_auto_managed(bool enable);
    void pause();
    void resume();

    void dump(std::stringstream &ss);

    void save_resume_data() const;
    void save_torrent_file() const;
    std::vector<char> generate() const;

    std::int64_t memory_size() const;
    void memory_size(std::int64_t size);

    int readahead_pieces() const;

    bool has_readers() const;
    std::map<std::int64_t, Reader*> readers() const;
    void register_reader(std::int64_t id, Reader* reader);
    void unregister_reader(const std::int64_t &id);

    void prioritize();
    void update_buffer_progress();
};

} // namespace lh