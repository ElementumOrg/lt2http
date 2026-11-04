#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>

#include <boost/dynamic_bitset.hpp>

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
#include <bittorrent/torrent_state.h>

using Bitset = boost::dynamic_bitset<>;

namespace lh {

class Reader;
class File;

struct TrackerInfo {
    std::string lastMessage;
    int numPeers = 0;
};

class Session;

struct TrackerStatus {
    std::string url;
    // -1 for DHT
    int tier = -1;
    // working, updating, failed, not_contacted or off
    std::string status;
    int fails = 0;
    // -1 when unknown
    int seeds = -1;
    int peers = -1;
    int downloaded = -1;
    // Seconds
    int next_announce = -1;
    std::string message;
};

struct PiecesStatus {
    // A character per piece: * stored and read by a stream, ? wanted by a stream, + stored, ~ downloading,
    // 1-7 queued with that priority, - not wanted
    std::string map;
    int total = 0;
    int stored = 0;
    int downloading = 0;
    int prioritized = 0;
    int readers = 0;
};

// Torrent state read by HTTP and background threads. The alerts thread replaces it as a whole,
// so readers never call libtorrent for it and never see a half updated state.
struct TorrentSnapshot {
    lt::torrent_status status;
    // Null until metadata is received
    std::shared_ptr<const lt::torrent_info> info;

    bool has_metadata() const { return info && info->is_valid() && info->num_files() > 0; }
    // As of the last status update, live checks use Torrent::have_piece()
    bool have_piece(int piece) const;
};

class Torrent : public std::enable_shared_from_this<lh::Torrent> {
  private:
    std::shared_ptr<lt::session> m_nativeSession;
    lt::torrent_handle m_nativeHandle;
    lh::storage_type_t m_storageType;
    std::string m_hash;

    std::shared_ptr<const TorrentSnapshot> m_snapshot;
    mutable std::mutex m_snapshotMutex;

    // libtorrent handle and storage calls run under a shared lock, remove() takes it exclusively,
    // so it waits for running calls, and calls after it see m_isClosing and do nothing.
    mutable std::shared_mutex m_handleMutex;
    std::atomic<bool> m_isClosing{false};
    // Set once metadata is received, used under m_handleMutex
    lt::storage_interface *m_storage = nullptr;

    // Files are replaced on the alerts thread, readers are (un)registered on HTTP threads
    // and iterated on the prioritizer thread.
    std::vector<std::shared_ptr<File>> m_files;
    mutable std::mutex m_filesMutex;
    std::map<std::int64_t, Reader *> m_readers;
    mutable std::mutex m_readersMutex;

    // Guards the tracker infos and deadline pieces
    mutable std::mutex m_mutex;
    std::map<std::string, TrackerInfo> m_trackerInfos;
    Bitset m_deadline_pieces;

    std::string m_torrentFile;
    std::string m_resumeFile;
    std::string m_partsFile;
    std::string m_metaFile;

    // Free JSON set by API clients, see meta_info()
    std::string m_metaInfo;
    // Players registered with the API, see add_player()
    std::atomic<int> m_players{0};
    // Used only by the prioritizer thread, see expire_players()
    std::chrono::steady_clock::time_point m_playersIdleSince = std::chrono::steady_clock::now();

    std::atomic<bool> m_isStopped{false};
    mutable std::atomic<int> m_pendingResumeData{0};

    std::chrono::time_point<std::chrono::system_clock> m_addedTime;

    void set_snapshot(const lt::torrent_status &status, std::shared_ptr<const lt::torrent_info> info);
    void log_handle_error(const std::exception &e) const;
    memory_storage *memory() const { return is_memory_storage() ? static_cast<memory_storage *>(m_storage) : nullptr; }

    // Runs f(handle) while the torrent is in the session, returns a default value when it is removed
    // (or being removed) or when libtorrent throws. f must not call with_handle() again.
    template <typename F> auto with_handle(F &&f) const {
        using R = std::invoke_result_t<F, const lt::torrent_handle &>;
        std::shared_lock lock(m_handleMutex);
        if (m_isClosing || !m_nativeHandle.is_valid())
            return R();
        try {
            return f(m_nativeHandle);
        } catch (const std::exception &e) {
            log_handle_error(e);
            return R();
        }
    }

  public:
    Torrent(std::shared_ptr<lt::session> nativeSession, const lt::torrent_handle &nativeHandle, lh::storage_type_t st,
            std::chrono::time_point<std::chrono::system_clock> added_time);
    ~Torrent();

    std::shared_ptr<const TorrentSnapshot> snapshot() const;
    std::vector<std::shared_ptr<File>> files() const;
    std::shared_ptr<File> get_file(int index);

    bool is_closing() const;
    const std::string &hash() const;
    std::string name() const;
    std::int64_t size() const;
    // Folder of the files on disk, empty for memory storage
    std::string download_path() const;
    int piece_length() const;
    int piece_length_at(int index) const;
    int pieces_count() const;
    bool have_piece(lt::piece_index_t piece) const;
    // Reads from the storage, 0 bytes once the torrent is removed
    int read_piece(lt::iovec_t buffer, lt::piece_index_t piece, int offset, lt::storage_error &ec) const;

    bool has_metadata() const;
    void update_metadata();
    void on_metadata_received();
    void remove(bool is_delete_files, bool is_delete_data);
    bool is_memory_storage() const;

    void update_status(const lt::torrent_status &status);
    TorrentState state() const;
    std::string state_string() const;

    bool is_buffering() const;
    bool is_paused() const;

    std::int64_t upload_rate() const;
    std::int64_t download_rate() const;
    std::int64_t total() const;
    std::int64_t total_done() const;
    std::int64_t total_wanted() const;
    std::int64_t total_wanted_done() const;
    std::int64_t total_download() const;
    std::int64_t total_upload() const;

    std::int64_t active_time() const;
    std::int64_t finished_time() const;
    std::int64_t seeding_time() const;

    int seeds_count() const;
    int peers_count() const;
    int total_seeds_count() const;
    int total_peers_count() const;

    double progress() const;
    double progress_total() const;
    double progress_wanted() const;
    std::vector<double> files_progress() const;
    std::vector<std::int64_t> files_completed_bytes() const;

    lt::download_priority_t file_priority(lt::file_index_t index) const;
    void file_priority(lt::file_index_t index, lt::download_priority_t priority);
    // Sets the priority of all files
    void files_priority(lt::download_priority_t priority);
    int files_chosen() const;
    lt::download_priority_t piece_priority(lt::piece_index_t index) const;
    void piece_priority(lt::piece_index_t index, lt::download_priority_t priority);
    void set_piece_deadline(lt::piece_index_t index, int deadline);
    void clear_piece_deadline(lt::piece_index_t index);
    void prioritize_pieces(lt::piece_index_t start, lt::piece_index_t end);
    void set_piece_priority(int piece, int deadline, lt::download_priority_t priority);
    void update_reserved_pieces(const std::vector<int> &pieces);

    void dispatch_alert(const lt::alert *a);
    void handle_save_resume_data_alert(const lt::save_resume_data_alert *p);
    void update_tracker(const std::string &name, std::string message, int number);
    void pause();
    void resume();

    // Trackers, with DHT last
    std::vector<TrackerStatus> trackers_status() const;
    PiecesStatus pieces_status() const;
    // The torrent for /info, in a language like en_gb
    void dump(std::stringstream &ss, bool is_files, bool is_trackers, bool is_pieces, const std::string &lang);

    void save_resume_data() const;
    // Saves requested, their save_resume_data(_failed)_alert not handled yet
    bool has_pending_resume_data() const;
    void save_torrent_file() const;
    std::vector<char> generate() const;

    std::int64_t memory_size() const;
    void memory_size(std::int64_t size);

    int readahead_pieces() const;

    bool has_readers() const;
    // Active stream connections, of one file or of all with file_index -1
    int readers_count(int file_index = -1) const;
    void register_reader(Reader *reader);
    void unregister_reader(std::int64_t id);

    void prioritize();
    void update_buffer_progress();

    // JSON text, empty if not set
    std::string meta_info() const;
    // Takes valid JSON text, saved with the torrent for file storage
    void meta_info(const std::string &json);

    int players() const;
    void add_player();
    void remove_player();
    // Called by the prioritizer: removes players after player_timeout without stream connections
    void expire_players();
};

} // namespace lh
