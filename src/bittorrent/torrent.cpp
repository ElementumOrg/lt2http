#include "torrent.h"
#include "utils/exceptions.h"

#include <algorithm>
#include <libtorrent/bencode.hpp>
#include <libtorrent/download_priority.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <utility>

#include <oatpp/core/Types.hpp>
#include <oatpp/core/base/Environment.hpp>

#include <app/application.h>

#include <bittorrent/file.h>
#include <bittorrent/reader.h>
#include <bittorrent/session.h>

#include <utils/numbers.h>
#include <utils/path.h>
#include <utils/strings.h>

namespace lh {

bool TorrentSnapshot::have_piece(int piece) const {
    if (status.is_seeding)
        return true;
    return piece >= 0 && piece < status.pieces.size() && status.pieces.get_bit(lt::piece_index_t{piece});
}

Torrent::Torrent(std::shared_ptr<lt::session> nativeSession, const lt::torrent_handle &nativeHandle, const lh::storage_type_t st,
                 std::chrono::time_point<std::chrono::system_clock> added_time)
    : m_nativeSession(std::move(nativeSession)), m_nativeHandle(nativeHandle), m_storageType(st),
      m_hash(to_hex(m_nativeHandle.info_hash())), m_addedTime(added_time) {
    auto &config = lh::config();

    m_torrentFile = torrent_file(config.torrents_path, m_hash);
    m_resumeFile = resume_file(config.torrents_path, m_hash);
    m_partsFile = parts_file(config.download_path, m_hash);

    // Later updates come from the alerts thread, see Session::handle_state_update_alert()
    set_snapshot(m_nativeHandle.status(), m_nativeHandle.torrent_file());

    OATPP_LOGI("Torrent", "Creating torrent with hash: %s", hash().c_str());
}

Torrent::~Torrent() { OATPP_LOGI("Torrent", "Destroying: %s", hash().c_str()); }

void Torrent::log_handle_error(const std::exception &e) const {
    OATPP_LOGE("Torrent", "libtorrent call failed for torrent %s: %s", m_hash.c_str(), e.what());
}

std::shared_ptr<const TorrentSnapshot> Torrent::snapshot() const {
    std::scoped_lock guard(m_snapshotMutex);
    return m_snapshot;
}

void Torrent::set_snapshot(const lt::torrent_status &status, std::shared_ptr<const lt::torrent_info> info) {
    auto snapshot = std::make_shared<TorrentSnapshot>();
    snapshot->status = status;
    snapshot->info = std::move(info);
    snapshot->state = compute_state(snapshot->status, snapshot->has_metadata());

    std::scoped_lock guard(m_snapshotMutex);
    m_snapshot = std::move(snapshot);
}

void Torrent::update_status(const lt::torrent_status &status) { set_snapshot(status, snapshot()->info); }

std::vector<std::shared_ptr<File>> Torrent::files() const {
    std::scoped_lock guard(m_filesMutex);
    return m_files;
}

std::shared_ptr<File> Torrent::get_file(int index) {
    std::scoped_lock guard(m_filesMutex);
    auto match = std::find_if(m_files.begin(), m_files.end(), [&index](const std::shared_ptr<File> &t) { return t->index() == index; });
    if (match == m_files.end())
        throw lh::FileException(Fmt("File with index '%d' not found in torrent with hash '%s'", index, hash().c_str()));

    return *match;
}

bool Torrent::is_closing() const { return m_isClosing; }

std::string Torrent::name() const {
    auto s = snapshot();
    return s->has_metadata() ? s->info->name() : s->status.name;
}

const std::string &Torrent::hash() const { return m_hash; }

std::int64_t Torrent::size() const {
    auto s = snapshot();
    return s->has_metadata() ? s->info->total_size() : -1;
}

int Torrent::piece_length() const {
    auto s = snapshot();
    return s->has_metadata() ? s->info->piece_length() : -1;
}

int Torrent::piece_length_at(const int index) const {
    auto s = snapshot();
    return s->has_metadata() ? s->info->piece_size(lt::piece_index_t{index}) : -1;
}

int Torrent::pieces_count() const {
    auto s = snapshot();
    return s->has_metadata() ? s->info->num_pieces() : -1;
}

bool Torrent::have_piece(lt::piece_index_t piece) const {
    return with_handle([piece](const lt::torrent_handle &h) { return h.have_piece(piece); });
}

int Torrent::read_piece(lt::iovec_t buffer, lt::piece_index_t piece, int offset, lt::storage_error &ec) const {
    return with_handle(
        [&](const lt::torrent_handle &) { return m_storage ? m_storage->readv(buffer, piece, offset, lt::open_mode::read_only, ec) : 0; });
}

bool Torrent::has_metadata() const { return snapshot()->has_metadata(); }

void Torrent::update_metadata() {
    auto info = with_handle([](const lt::torrent_handle &h) { return h.torrent_file(); });
    set_snapshot(snapshot()->status, std::move(info));

    on_metadata_received();
}

void Torrent::on_metadata_received() {
    auto info = snapshot()->info;
    if (!info)
        return;

    save_torrent_file();
    save_resume_data();

    auto storage = with_handle([this](const lt::torrent_handle &h) {
        auto *storage = h.get_storage_impl();
        if (is_memory_storage())
            static_cast<memory_storage *>(storage)->set_torrent_handle(h);
        return storage;
    });
    {
        std::unique_lock lock(m_handleMutex);
        m_storage = storage;
    }
    {
        std::scoped_lock guard(m_mutex);
        m_deadline_pieces.resize(info->num_pieces());
    }

    auto priorities = with_handle([](const lt::torrent_handle &h) { return h.get_file_priorities(); });
    auto const &storage_files = info->files();

    std::vector<std::shared_ptr<File>> files;
    for (int i = 0; i < info->num_files(); i++) {
        auto priority = std::size_t(i) < priorities.size() ? priorities[i] : lt::dont_download;
        files.emplace_back(std::make_shared<lh::File>(this, lt::file_index_t(i), storage_files.file_path(i),
                                                      storage_files.file_name(i).to_string(), storage_files.file_size(i),
                                                      storage_files.file_offset(i), priority));
    }

    std::scoped_lock guard(m_filesMutex);
    m_files = std::move(files);
}

void Torrent::remove(bool is_delete_files, bool is_delete_data) {
    m_isClosing = true;

    auto flags = is_delete_data && !is_memory_storage() ? lt::session::delete_files : lt::session::delete_partfile;
    {
        // Waits for running libtorrent and storage calls, later ones see m_isClosing
        std::unique_lock lock(m_handleMutex);
        try {
            m_nativeSession->remove_torrent(m_nativeHandle, flags);
        } catch (const std::exception &e) {
            log_handle_error(e);
        }
        m_storage = nullptr;
    }

    auto remove = [](const std::string &path) {
        if (file_exists(path)) {
            OATPP_LOGI("Torrent::remove", "Removing file at: %s", path.c_str())
            remove_file(path);
        }
    };

    if (is_delete_files)
        remove(m_torrentFile);

    if (is_delete_files || is_memory_storage()) {
        remove(m_resumeFile);
        remove(m_partsFile);
    }

    OATPP_LOGI("Torrent::remove", "Removed torrent with hash: %s", hash().c_str());
}

bool Torrent::is_memory_storage() const { return m_storageType == storage_type_t::memory; }

TorrentState Torrent::state() const { return snapshot()->state; }

std::string Torrent::state_string() const {
    switch (state()) {
    case TorrentState::Unknown:
        return "Unknown";
    case TorrentState::Queued:
        return "Queued";
    case TorrentState::ForcedDownloading:
        return "Forced downloading";
    case TorrentState::Downloading:
        return "Downloading";
    case TorrentState::DownloadingMetadata:
        return "Downloading metadata";
    case TorrentState::StalledDownloading:
        return "Stalled downloading";
    case TorrentState::ForcedUploading:
        return "Forced uploading";
    case TorrentState::Uploading:
        return "Uploading";
    case TorrentState::StalledUploading:
        return "Stalled uploading";
    case TorrentState::CheckingResumeData:
        return "Checking resume data";
    case TorrentState::QueuedDownloading:
        return "Queued downloading";
    case TorrentState::QueuedUploading:
        return "Queued uploading";
    case TorrentState::CheckingUploading:
        return "Checking uploading";
    case TorrentState::CheckingDownloading:
        return "Checking downloading";
    case TorrentState::PausedDownloading:
        return "Paused downloading";
    case TorrentState::PausedUploading:
        return "Paused uploading";
    case TorrentState::Moving:
        return "Moving";
    case TorrentState::MissingFiles:
        return "Missing files";
    case TorrentState::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

TorrentState Torrent::compute_state(const lt::torrent_status &status, bool has_metadata) const {
    const bool paused = is_paused();
    const bool auto_managed = static_cast<bool>(status.flags & lt::torrent_flags::auto_managed);
    const bool native_paused = static_cast<bool>(status.flags & lt::torrent_flags::paused);

    if (status.state == lt::torrent_status::checking_resume_data)
        return TorrentState::CheckingResumeData;
    if (status.errc)
        return TorrentState::Error;
    if (!has_metadata) {
        if (paused)
            return TorrentState::PausedDownloading;
        // Queued: not paused by us, but paused by libtorrent's queue
        if (auto_managed && native_paused)
            return TorrentState::QueuedDownloading;
        return TorrentState::DownloadingMetadata;
    }
    // Being checked, not just in the "checking" state
    if (status.state == lt::torrent_status::checking_files && (!paused || auto_managed || !native_paused))
        return TorrentState::CheckingDownloading;
    if (status.total_wanted == 0)
        return TorrentState::Queued;
    if (status.state == lt::torrent_status::finished || status.state == lt::torrent_status::seeding) {
        if (paused)
            return TorrentState::PausedUploading;
        // Forced: not paused, as the queueing system is not used
        return TorrentState::ForcedUploading;
    }
    if (paused)
        return TorrentState::PausedDownloading;
    return TorrentState::ForcedDownloading;
}

bool Torrent::is_buffering() const {
    auto all = files();
    return std::any_of(all.begin(), all.end(),
                       [](const std::shared_ptr<File> &f) { return f->is_buffering() && f->buffer_progress() < 100; });
}

bool Torrent::is_paused() const { return m_isStopped; }

std::int64_t Torrent::upload_rate() const { return snapshot()->status.upload_payload_rate; }

std::int64_t Torrent::download_rate() const { return snapshot()->status.download_payload_rate; }

std::int64_t Torrent::total() const { return size(); }

std::int64_t Torrent::total_done() const { return snapshot()->status.total_done; }

std::int64_t Torrent::total_wanted() const { return snapshot()->status.total_wanted; }

std::int64_t Torrent::total_wanted_done() const { return snapshot()->status.total_wanted_done; }

std::int64_t Torrent::total_download() const { return snapshot()->status.all_time_download; }

std::int64_t Torrent::total_upload() const { return snapshot()->status.all_time_upload; }

std::int64_t Torrent::active_time() const { return lt::total_seconds(snapshot()->status.active_duration); }

std::int64_t Torrent::finished_time() const { return lt::total_seconds(snapshot()->status.finished_duration); }

std::int64_t Torrent::seeding_time() const { return lt::total_seconds(snapshot()->status.seeding_duration); }

int Torrent::seeds_count() const { return snapshot()->status.num_seeds; }

int Torrent::peers_count() const { return snapshot()->status.num_peers; }

int Torrent::total_seeds_count() const {
    auto const &status = snapshot()->status;
    return status.num_complete > 0 ? status.num_complete : status.list_seeds;
}

int Torrent::total_peers_count() const {
    auto const &status = snapshot()->status;
    const int peers = status.num_complete + status.num_incomplete;
    return peers > 0 ? peers : status.list_peers;
}

double Torrent::progress() const {
    auto const &status = snapshot()->status;
    if (status.state == lt::torrent_status::checking_files || status.state == lt::torrent_status::checking_resume_data)
        return status.progress;

    if (status.total_wanted == 0)
        return 0.;

    if (status.total_wanted_done == status.total_wanted)
        return 1.;

    return static_cast<double>(status.total_wanted_done) / status.total_wanted;
}

double Torrent::progress_total() const {
    auto total_size = total();
    if (total_size <= 0)
        return 0;

    return 100 * (double(total_done()) / double(total_size));
}

double Torrent::progress_wanted() const {
    auto const &status = snapshot()->status;
    if (status.total_wanted <= 0)
        return 0;

    return 100 * (double(status.total_wanted_done) / double(status.total_wanted));
}

std::vector<double> Torrent::files_progress() const {
    auto fp = files_completed_bytes();
    auto all = files();
    const std::size_t count = std::min(fp.size(), all.size());
    std::vector<double> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::int64_t size = all[i]->size();
        if ((size <= 0) || (fp[i] == size))
            result.push_back(1);
        else
            result.push_back(fp[i] / static_cast<double>(size));
    }

    return result;
}

std::vector<std::int64_t> Torrent::files_completed_bytes() const {
    if (!has_metadata())
        return {};

    return with_handle([](const lt::torrent_handle &h) {
        std::vector<std::int64_t> fp;
        h.file_progress(fp, lt::torrent_handle::piece_granularity);
        return fp;
    });
}

lt::download_priority_t Torrent::file_priority(lt::file_index_t index) const {
    return with_handle([index](const lt::torrent_handle &h) { return h.file_priority(index); });
}

void Torrent::file_priority(lt::file_index_t index, lt::download_priority_t priority) {
    if (is_memory_storage())
        return;

    OATPP_LOGI("Torrent::file_priority", "Setting priority '%d' for file '%d'", priority, index);
    with_handle([&](const lt::torrent_handle &h) { h.file_priority(index, priority); });
    save_resume_data();
}

lt::download_priority_t Torrent::piece_priority(lt::piece_index_t index) const {
    return with_handle([index](const lt::torrent_handle &h) { return h.piece_priority(index); });
}

void Torrent::piece_priority(lt::piece_index_t index, lt::download_priority_t priority) {
    OATPP_LOGI("Torrent::piece_priority", "Setting priority '%d' for piece '%d'", priority, index);
    with_handle([&](const lt::torrent_handle &h) { h.piece_priority(index, std::clamp(priority, lt::dont_download, lt::top_priority)); });
}

void Torrent::set_piece_deadline(lt::piece_index_t index, int deadline) {
    {
        std::scoped_lock guard(m_mutex);
        if (std::size_t(int(index)) >= m_deadline_pieces.size() || m_deadline_pieces.test(index)) {
            OATPP_LOGI("Torrent::set_piece_deadline", "Skipping deadline for piece '%d'", index);
            return;
        }
        m_deadline_pieces.set(index);
    }

    OATPP_LOGI("Torrent::set_piece_deadline", "Setting deadline '%d' for piece '%d'", deadline, index);
    with_handle([&](const lt::torrent_handle &h) { h.set_piece_deadline(index, deadline); });
}

void Torrent::clear_piece_deadline(lt::piece_index_t index) {
    std::scoped_lock guard(m_mutex);
    if (std::size_t(int(index)) < m_deadline_pieces.size())
        m_deadline_pieces.reset(index);
}

void Torrent::prioritize_pieces(lt::piece_index_t start, lt::piece_index_t end) {
    for (int i = start; i <= end; ++i) {
        if (have_piece(i))
            continue;

        if (i == int(start)) {
            set_piece_priority(i, 0, lt::download_priority_t{7});
        } else {
            set_piece_priority(i, (i - int(start)) * 50, lt::download_priority_t{6});
        }
    }
}

void Torrent::set_piece_priority(int piece, int deadline, lt::download_priority_t priority) {
    if (piece_priority(piece) < priority) {
        piece_priority(piece, priority);
        set_piece_deadline(piece, deadline);
    }
}

void Torrent::update_reserved_pieces(const std::vector<int> &pieces) {
    with_handle([&](const lt::torrent_handle &) {
        if (auto *storage = memory())
            storage->update_reserved_pieces(pieces);
    });
}

void Torrent::dispatch_alert(const lt::alert *a) {
    switch (a->type()) {
    case lt::save_resume_data_alert::alert_type:
        handle_save_resume_data_alert(static_cast<const lt::save_resume_data_alert *>(a));
        break;
    case lt::dht_reply_alert::alert_type:
        update_tracker("DHT", "", static_cast<const lt::dht_reply_alert *>(a)->num_peers);
        break;
    case lt::tracker_reply_alert::alert_type: {
        auto *p = static_cast<const lt::tracker_reply_alert *>(a);
        update_tracker(p->tracker_url(), p->message(), p->num_peers);
        break;
    }
    case lt::tracker_warning_alert::alert_type: {
        auto *p = static_cast<const lt::tracker_warning_alert *>(a);
        update_tracker(p->tracker_url(), p->warning_message(), 0);
        break;
    }
    case lt::tracker_error_alert::alert_type: {
        auto *p = static_cast<const lt::tracker_error_alert *>(a);
        update_tracker(p->tracker_url(), p->error_message(), 0);
        break;
    }
    }
}

void Torrent::handle_save_resume_data_alert(const lt::save_resume_data_alert *p) {
    OATPP_LOGI("Torrent::handle_save_resume_data_alert", "Saving resume data to: %s", m_resumeFile.c_str());

    auto const b = lt::write_resume_data_buf(p->params);
    if (!save_file(m_resumeFile, b.data(), b.size()))
        OATPP_LOGE("Torrent::handle_save_resume_data_alert", "Could not save resume data to: %s", m_resumeFile.c_str());
}

void Torrent::pause() {
    m_isStopped = true;

    with_handle([](const lt::torrent_handle &h) {
        h.unset_flags(lt::torrent_flags::auto_managed);
        h.pause();
    });
}

void Torrent::resume() {
    bool was_stopped = m_isStopped.exchange(false);
    bool has_error = static_cast<bool>(snapshot()->status.errc);

    with_handle([&](const lt::torrent_handle &h) {
        if (has_error)
            h.clear_error();
        // Torrent may have been temporarily resumed to perform checking files
        // so we have to ensure it will not pause after checking is done.
        if (was_stopped)
            h.unset_flags(lt::torrent_flags::stop_when_ready);
        h.set_flags(lt::torrent_flags::auto_managed);
        h.resume();
    });
}

void Torrent::update_tracker(const std::string &name, std::string message, int number) {
    std::scoped_lock guard(m_mutex);
    auto &info = m_trackerInfos[name];
    info.lastMessage = std::move(message);
    info.numPeers = std::max(number, info.numPeers);
}

void Torrent::dump(std::stringstream &ss) {
    auto yes_no = [](bool value) { return value ? "true" : "false"; };
    auto snap = snapshot();
    auto const &status = snap->status;

    ss << Fmt("    Name:               %s \n", name().c_str());
    ss << Fmt("    Storage:            %s \n", storage_to_string(m_storageType));
    ss << Fmt("    Infohash:           %s \n", hash().c_str());
    ss << Fmt("    Status:             %s \n", state_string().c_str());
    ss << Fmt("    Pieces count:       %d \n", pieces_count());
    ss << Fmt("    Piece length:       %s \n", humanize_bytes(piece_length()).c_str());
    ss << "\n";
    ss << "    Speed:\n";
    ss << Fmt("        Download:   %s/s \n", humanize_bytes(status.download_payload_rate).c_str());
    ss << Fmt("        Upload:     %s/s \n", humanize_bytes(status.upload_payload_rate).c_str());
    ss << "\n";
    ss << "    Size:\n";
    ss << Fmt("        Total:                  %s \n", humanize_bytes(total()).c_str());
    ss << Fmt("        Done:                   %s (%.2f%%) \n", humanize_bytes(status.total_done).c_str(), progress_total());
    ss << Fmt("        Wanted:                 %s \n", humanize_bytes(status.total_wanted).c_str());
    ss << Fmt("        Wanted done:            %s (%.2f%%) \n", humanize_bytes(status.total_wanted_done).c_str(), progress_wanted());
    ss << "\n";
    ss << "    Connections:\n";
    ss << Fmt("        Connections:            %d \n", status.num_connections);
    ss << Fmt("        Connections limit:      %d \n", status.connections_limit);
    ss << Fmt("        Connected seeds:        %d \n", status.num_seeds);
    ss << Fmt("        Connected peers:        %d \n", status.num_peers);
    ss << Fmt("        All Seeds:              %d \n", total_seeds_count());
    ss << Fmt("        All Peers:              %d \n", total_peers_count());

    ss << "    Flags:\n";
    ss << Fmt("        paused: %s \n", yes_no(status.flags & lt::torrent_flags::paused));
    ss << Fmt("        auto_managed: %s \n", yes_no(status.flags & lt::torrent_flags::auto_managed));
    ss << Fmt("        sequential_download: %s \n", yes_no(status.flags & lt::torrent_flags::sequential_download));
    ss << Fmt("        need_save_resume: %s \n", yes_no(status.flags & lt::torrent_flags::need_save_resume));
    ss << Fmt("        is_seeding: %s \n", yes_no(status.is_seeding));
    ss << Fmt("        is_finished: %s \n", yes_no(status.is_finished));
    ss << Fmt("        has_metadata: %s \n", yes_no(status.has_metadata));
    ss << Fmt("        has_incoming: %s \n", yes_no(status.has_incoming));
    ss << Fmt("        moving_storage: %s \n", yes_no(status.moving_storage));
    ss << Fmt("        announcing_to_trackers: %s \n", yes_no(status.announcing_to_trackers));
    ss << Fmt("        announcing_to_lsd: %s \n", yes_no(status.announcing_to_lsd));
    ss << Fmt("        announcing_to_dht: %s \n", yes_no(status.announcing_to_dht));
    ss << "\n";

    ss << "    Files (Priority):\n";
    auto file_priorities = with_handle([](const lt::torrent_handle &h) { return h.get_file_priorities(); });
    for (auto &f : files()) {
        auto pr = std::size_t(f->index()) < file_priorities.size() ? file_priorities[f->index()] : lt::dont_download;
        if (pr > 0) {
            ss << Fmt("        <a href=\"%s\" target=_blank>%s</a> (%s): %d \n", f->stream_uri().c_str(), f->path().c_str(),
                      humanize_bytes(f->size()).c_str(), pr);
        } else {
            ss << Fmt("        <a href=\"%s\" target=_blank>%s</a> (%s): - \n", f->stream_uri().c_str(), f->path().c_str(),
                      humanize_bytes(f->size()).c_str());
        }
    }

    ss << "\n";
    ss << "    Trackers:\n";

    auto tracker_infos = [this] {
        std::scoped_lock guard(m_mutex);
        return m_trackerInfos;
    }();

    std::map<std::string, std::string> trackers;
    for (auto &tracker : with_handle([](const lt::torrent_handle &h) { return h.trackers(); })) {
        int scrape_complete = -1;
        int scrape_incomplete = -1;
        std::string message;

        const bool is_updating = std::any_of(tracker.endpoints.begin(), tracker.endpoints.end(),
                                             [](const lt::announce_endpoint &endpoint) { return endpoint.updating; });

        const bool is_working = std::any_of(tracker.endpoints.begin(), tracker.endpoints.end(),
                                            [](const lt::announce_endpoint &endpoint) { return endpoint.is_working(); });

        for (auto &endpoint : tracker.endpoints) {
            scrape_complete = std::max(scrape_complete, endpoint.scrape_complete);
            scrape_incomplete = std::max(scrape_incomplete, endpoint.scrape_incomplete);
            if (!endpoint.message.empty())
                message = endpoint.message;
        }

        if (auto info = tracker_infos.find(tracker.url); scrape_incomplete == -1 && info != tracker_infos.end())
            scrape_incomplete = info->second.numPeers;

        trackers.emplace(tracker.url,
                         Fmt("        %-60s: %-3s seeds, %-3s peers, updating: %-5s, is_working: %-5s, message: %s\n", tracker.url.c_str(),
                             peer_num_to_string(scrape_complete).c_str(), peer_num_to_string(scrape_incomplete).c_str(),
                             yes_no(is_updating), yes_no(is_working), message.c_str()));
    }

    // List trackers information sorted by tracker url.
    for (const auto &[url, line] : trackers)
        ss << line;

    if (auto dht = tracker_infos.find("DHT"); dht != tracker_infos.end()) {
        auto const &entry = dht->second;
        ss << Fmt("        %-60s: %-3s seeds, %-3s peers, updating: %-5s, is_working: %-5s, message: %s\n", "DHT", "-",
                  peer_num_to_string(entry.numPeers).c_str(), "false", "true", entry.lastMessage.c_str());
    }
    ss << "\n";

    ss << "    Pieces:\n";
    ss << "        ";

    std::vector<int> reader_positions;
    {
        std::scoped_lock guard(m_readersMutex);
        for (const auto &[id, reader] : m_readers)
            reader_positions.push_back(reader->piece_start());
    }

    // One call for all priorities, pieces come from the snapshot
    auto piece_priorities = with_handle([](const lt::torrent_handle &h) { return h.get_piece_priorities(); });
    auto priority_of = [&](int i) { return std::size_t(i) < piece_priorities.size() ? int(piece_priorities[i]) : 0; };

    int total_prioritized = 0;
    int total_have = 0;
    for (int i = 0; i < pieces_count(); i++) {
        if (std::find(reader_positions.begin(), reader_positions.end(), i) != reader_positions.end()) {
            total_prioritized++;
            // Piece is in scope of Readers
            if (snap->have_piece(i)) {
                total_have++;
                ss << "<b>*</b>";
            } else {
                ss << "<b>?</b>";
            }
        } else if (snap->have_piece(i)) {
            // We already have this piece
            total_have++;
            if (priority_of(i) > 0)
                total_prioritized++;

            ss << "+";
        } else if (priority_of(i) == 0) {
            // Piece is not prioritized
            ss << "-";
        } else {
            // Piece is in the queue
            total_prioritized++;
            ss << priority_of(i);
        }

        if (i > 0 && (i + 1) % 100 == 0) {
            ss << "\n        ";
        }
    }
    ss << Fmt("\n        Readers: <b>%d</b>, Stored: <b>%d</b>, Prioritized: <b>%d</b>", int(reader_positions.size()), total_have,
              total_prioritized);
    ss << "\n\n\n";
    ss << "    " << std::string(150, '*') << "\n";
    ss << "\n\n";
}

void Torrent::save_resume_data() const {
    // Resume files are needed only for non-memory storage to keep the history and state.
    if (!is_memory_storage())
        with_handle([](const lt::torrent_handle &h) { h.save_resume_data(); });
}

void Torrent::save_torrent_file() const {
    if (!has_metadata() || is_memory_storage() || file_exists(m_torrentFile))
        return;

    OATPP_LOGI("Torrent::save_torrent_file", "Saving torrent file to: %s", m_torrentFile.c_str());

    auto data = generate();
    if (!save_file(m_torrentFile, data.data(), data.size()))
        OATPP_LOGE("Torrent::save_torrent_file", "Could not save torrent file to: %s", m_torrentFile.c_str());
}

std::vector<char> Torrent::generate() const {
    auto info = snapshot()->info;
    if (!info || !info->is_valid())
        throw lh::TorrentException("Torrent does not have metadata");

    OATPP_LOGI("Torrent::generate", "Generating torrent file for: %s", hash().c_str());

    std::vector<char> ret;
    lt::bencode(std::back_inserter(ret), lt::create_torrent(*info).generate());
    return ret;
}

std::int64_t Torrent::memory_size() const {
    if (!is_memory_storage())
        return lh::session().memory_size();

    return with_handle([this](const lt::torrent_handle &) -> std::int64_t {
        auto *storage = memory();
        return storage ? storage->get_memory_size() : 0;
    });
}

void Torrent::memory_size(std::int64_t size) {
    if (!is_memory_storage()) {
        lh::config().memory_size = int(size / 1024 / 1024);
        return;
    }

    with_handle([&](const lt::torrent_handle &) {
        if (auto *storage = memory())
            storage->set_memory_size(size);
    });
}

int Torrent::readahead_pieces() const {
    if (!is_memory_storage())
        // That is a static value of pieces with priority > 1 to set for file storage.
        return lh::file_readahead_pieces;

    return with_handle([this](const lt::torrent_handle &) {
        auto *storage = memory();
        return storage ? storage->get_buffers_count() * lh::config().readahead_percents / 100 : 0;
    });
}

bool Torrent::has_readers() const {
    std::scoped_lock guard(m_readersMutex);
    return !m_readers.empty();
}

void Torrent::register_reader(Reader *reader) {
    std::scoped_lock guard(m_readersMutex);
    m_readers[reader->id()] = reader;
}

void Torrent::unregister_reader(std::int64_t id) {
    std::scoped_lock guard(m_readersMutex);
    m_readers.erase(id);
}

void Torrent::prioritize() {
    // Held for the whole run: readers unregister in their destructor, so the pointers stay valid while locked.
    std::scoped_lock guard(m_readersMutex);

    int pieces_limit = readahead_pieces();
    int iter_count = -1;
    std::size_t readers_finished = 0;
    std::vector<int> reader_pieces;
    std::map<std::int64_t, bool> iterated;

    while (readers_finished < m_readers.size() && pieces_limit > 0) {
        iter_count++;
        for (const auto &[id, reader] : m_readers) {
            int index = reader->piece_start() + iter_count;
            if (iterated[id]) {
                continue;
            }
            if (reader->is_closing() || index > reader->piece_end_limit()) {
                iterated[id] = true;
                readers_finished++;
                continue;
            }

            if (std::find(reader_pieces.begin(), reader_pieces.end(), index) != reader_pieces.end())
                continue;

            reader_pieces.push_back(index);
            pieces_limit--;
            if (pieces_limit <= 0)
                break;
        }
    }

    // One call for all priorities, pieces come from the snapshot
    auto snap = snapshot();
    auto priorities = with_handle([](const lt::torrent_handle &h) { return h.get_piece_priorities(); });
    auto priority_of = [&](int piece) { return std::size_t(piece) < priorities.size() ? int(priorities[piece]) : 0; };

    // Remove non-needed priorities to release place for new pieces
    std::vector<std::pair<lt::piece_index_t, lt::download_priority_t>> pieces_request;
    if (is_memory_storage()) {
        for (std::size_t piece = 0; piece < priorities.size(); ++piece) {
            if (priorities[piece] <= 0)
                continue;
            if (std::find(reader_pieces.begin(), reader_pieces.end(), int(piece)) == reader_pieces.end())
                pieces_request.emplace_back(lt::piece_index_t(int(piece)), lt::dont_download);
        }
    }

    // Prioritize readehead pieces with priority groups, each next group should be bigger than previous,
    //  to have proper prioritization.
    int progression_priority = 5;
    int progression_start = 2;
    double progression_multiplier = 1.7;
    int progression_group = 1;
    int progression_left = progression_result(progression_start, progression_multiplier, progression_group);

    for (const auto &piece : reader_pieces) {
        if (progression_left <= 0 && progression_priority > 1) {
            progression_priority--;
            progression_group++;
            progression_left = progression_result(progression_start, progression_multiplier, progression_group);
        }

        progression_left--;
        int priority = std::max(2, progression_priority);
        if (!snap->have_piece(piece) && priority_of(piece) < priority)
            pieces_request.emplace_back(lt::piece_index_t(piece), lt::download_priority_t(priority));
    }

    if (!pieces_request.empty()) {
        OATPP_LOGI("Torrent::prioritize", "Prioritizing %d pieces out of %d possible", int(pieces_request.size()), readahead_pieces())
        with_handle([&](const lt::torrent_handle &h) { h.prioritize_pieces(pieces_request); });
    }

    with_handle([&](const lt::torrent_handle &) {
        if (auto *storage = memory())
            storage->update_reader_pieces(reader_pieces);
    });
}

void Torrent::update_buffer_progress() {
    if (!is_buffering())
        return;

    auto snap = snapshot();
    auto queue = with_handle([](const lt::torrent_handle &h) {
        std::vector<lt::partial_piece_info> pieces;
        h.get_download_queue(pieces);
        return pieces;
    });

    for (const auto &file : files()) {
        if (!file->is_buffering() || file->buffer_progress() >= 100 || file->buffer_size() <= 0)
            continue;

        auto buffer_pieces = file->buffer_pieces();
        auto buffer_size = file->buffer_size();
        std::int64_t missing = 0;

        for (const auto &piece : buffer_pieces) {
            if (!snap->have_piece(piece))
                missing += piece_length_at(piece);
        }

        if (missing > 0) {
            for (const auto &partial : queue) {
                if (std::find(buffer_pieces.begin(), buffer_pieces.end(), int(partial.piece_index)) != buffer_pieces.end()) {
                    for (int b = 0; b < partial.blocks_in_piece; b++)
                        missing -= partial.blocks[b].bytes_progress;
                }
            }
        }

        file->set_buffer_progress(double(buffer_size - missing) / buffer_size * 100);
    }
}

} // namespace lh
