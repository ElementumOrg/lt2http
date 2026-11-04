#include "torrent.h"
#include "utils/exceptions.h"

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/download_priority.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <utility>

#include <oatpp/core/Types.hpp>
#include <oatpp/core/base/Environment.hpp>

#include <app/application.h>
#include <app/i18n.h>

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
    m_metaFile = meta_file(config.torrents_path, m_hash);

    if (file_exists(m_metaFile) && !load_file(m_metaFile, m_metaInfo))
        OATPP_LOGE("Torrent", "Could not read meta info from: %s", m_metaFile.c_str());

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

std::string Torrent::download_path() const {
    auto s = snapshot();
    if (is_memory_storage() || s->status.save_path.empty())
        return "";
    // Files of a torrent with more than one file are in a folder named like the torrent
    if (s->has_metadata() && s->info->num_files() > 1)
        return path_append(s->status.save_path, s->info->name());
    return s->status.save_path;
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
        remove(m_metaFile);
    }

    OATPP_LOGI("Torrent::remove", "Removed torrent with hash: %s", hash().c_str());
}

bool Torrent::is_memory_storage() const { return m_storageType == storage_type_t::memory; }

// Not kept in the snapshot: buffering, players and pause change between status updates
TorrentState Torrent::state() const {
    auto s = snapshot();
    return compute_state(s->status, s->has_metadata(), is_paused(), is_buffering(), players());
}

std::string Torrent::state_string() const { return state_to_string(state()); }

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

    OATPP_LOGI("Torrent::file_priority", "Setting priority '%d' for file '%d'", int(priority), int(index));
    with_handle([&](const lt::torrent_handle &h) { h.file_priority(index, priority); });
    save_resume_data();
}

void Torrent::files_priority(lt::download_priority_t priority) {
    if (is_memory_storage())
        return;

    OATPP_LOGI("Torrent::files_priority", "Setting priority '%d' for all files", int(priority));
    with_handle([&](const lt::torrent_handle &h) {
        if (auto info = h.torrent_file())
            h.prioritize_files(std::vector<lt::download_priority_t>(std::size_t(info->num_files()), priority));
    });
    save_resume_data();
}

int Torrent::files_chosen() const {
    auto priorities = with_handle([](const lt::torrent_handle &h) { return h.get_file_priorities(); });
    return int(std::count_if(priorities.begin(), priorities.end(), [](lt::download_priority_t p) { return p > lt::dont_download; }));
}

lt::download_priority_t Torrent::piece_priority(lt::piece_index_t index) const {
    return with_handle([index](const lt::torrent_handle &h) { return h.piece_priority(index); });
}

void Torrent::piece_priority(lt::piece_index_t index, lt::download_priority_t priority) {
    OATPP_LOGI("Torrent::piece_priority", "Setting priority '%d' for piece '%d'", int(priority), int(index));
    with_handle([&](const lt::torrent_handle &h) { h.piece_priority(index, std::clamp(priority, lt::dont_download, lt::top_priority)); });
}

void Torrent::set_piece_deadline(lt::piece_index_t index, int deadline) {
    {
        std::scoped_lock guard(m_mutex);
        if (std::size_t(int(index)) >= m_deadline_pieces.size() || m_deadline_pieces.test(index)) {
            OATPP_LOGI("Torrent::set_piece_deadline", "Skipping deadline for piece '%d'", int(index));
            return;
        }
        m_deadline_pieces.set(index);
    }

    OATPP_LOGI("Torrent::set_piece_deadline", "Setting deadline '%d' for piece '%d'", deadline, int(index));
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
        m_pendingResumeData--;
        handle_save_resume_data_alert(static_cast<const lt::save_resume_data_alert *>(a));
        break;
    case lt::save_resume_data_failed_alert::alert_type:
        m_pendingResumeData--;
        OATPP_LOGE("Torrent::dispatch_alert", "Could not save resume data of %s: %s", m_hash.c_str(), a->message().c_str());
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

std::vector<TrackerStatus> Torrent::trackers_status() const {
    auto tracker_infos = [this] {
        std::scoped_lock guard(m_mutex);
        return m_trackerInfos;
    }();

    std::vector<TrackerStatus> result;
    auto const now = lt::clock_type::now();
    // In tier order, as libtorrent announces to them
    for (auto &tracker : with_handle([](const lt::torrent_handle &h) { return h.trackers(); })) {
        TrackerStatus row;
        row.url = tracker.url;
        row.tier = tracker.tier;
        bool is_updating = false;
        bool is_working = false;

        for (auto &endpoint : tracker.endpoints) {
            row.seeds = std::max(row.seeds, endpoint.scrape_complete);
            row.peers = std::max(row.peers, endpoint.scrape_incomplete);
            row.downloaded = std::max(row.downloaded, endpoint.scrape_downloaded);
            row.fails = std::max(row.fails, int(endpoint.fails));
            is_updating = is_updating || endpoint.updating;
            is_working = is_working || endpoint.is_working();
            if (endpoint.enabled) {
                auto in = int(std::chrono::duration_cast<std::chrono::seconds>(endpoint.next_announce - now).count());
                if (in >= 0 && (row.next_announce < 0 || in < row.next_announce))
                    row.next_announce = in;
            }
            if (endpoint.last_error)
                row.message = endpoint.last_error.message();
            else if (!endpoint.message.empty() && row.message.empty())
                row.message = endpoint.message;
        }

        if (auto info = tracker_infos.find(tracker.url); info != tracker_infos.end()) {
            if (row.peers == -1)
                row.peers = info->second.numPeers;
            if (row.message.empty())
                row.message = info->second.lastMessage;
        }

        row.status = is_updating ? "updating" : is_working ? "working" : row.fails > 0 ? "failed" : "not_contacted";
        result.push_back(std::move(row));
    }

    if (auto dht = tracker_infos.find("DHT"); dht != tracker_infos.end()) {
        TrackerStatus row;
        row.url = "DHT";
        row.status = snapshot()->status.announcing_to_dht ? "working" : "off";
        row.peers = dht->second.numPeers;
        row.message = dht->second.lastMessage;
        result.push_back(std::move(row));
    }
    return result;
}

PiecesStatus Torrent::pieces_status() const {
    PiecesStatus result;
    auto snap = snapshot();
    result.total = std::max(0, pieces_count());

    std::vector<int> reader_positions;
    {
        std::scoped_lock guard(m_readersMutex);
        for (const auto &[id, reader] : m_readers)
            reader_positions.push_back(reader->piece_start());
    }
    result.readers = int(reader_positions.size());

    // One call for all priorities and one for the pieces in download, stored pieces come from the snapshot
    auto piece_priorities = with_handle([](const lt::torrent_handle &h) { return h.get_piece_priorities(); });
    auto priority_of = [&](int i) { return std::size_t(i) < piece_priorities.size() ? int(piece_priorities[i]) : 0; };
    auto queue = with_handle([](const lt::torrent_handle &h) {
        std::vector<lt::partial_piece_info> pieces;
        h.get_download_queue(pieces);
        return pieces;
    });
    std::vector<bool> is_downloading(std::size_t(result.total));
    for (const auto &partial : queue) {
        if (std::size_t(int(partial.piece_index)) < is_downloading.size())
            is_downloading[int(partial.piece_index)] = true;
    }

    result.map.reserve(std::size_t(result.total));
    for (int i = 0; i < result.total; i++) {
        const bool have = snap->have_piece(i);
        const int priority = priority_of(i);
        if (have)
            result.stored++;
        if (!have && is_downloading[i])
            result.downloading++;

        if (std::find(reader_positions.begin(), reader_positions.end(), i) != reader_positions.end()) {
            result.prioritized++;
            result.map += have ? '*' : '?';
        } else if (have) {
            if (priority > 0)
                result.prioritized++;
            result.map += '+';
        } else if (is_downloading[i]) {
            result.prioritized++;
            result.map += '~';
        } else if (priority == 0) {
            result.map += '-';
        } else {
            result.prioritized++;
            result.map += char('0' + std::min(priority, 7));
        }
    }
    return result;
}

void Torrent::dump(std::stringstream &ss, bool is_files, bool is_trackers, bool is_pieces, const std::string &lang) {
    auto yes_no = [](bool value) { return value ? "true" : "false"; };
    auto tr = [&lang](const char *key, std::initializer_list<std::string> args = {}) { return html_escape(translate(lang, key, args)); };
    // A translated label padded to the width of the column, so the values line up
    auto label = [&tr](const char *key, std::size_t width) { return pad_right(tr(key) + ":", width); };
    auto snap = snapshot();
    auto const &status = snap->status;
    const bool is_memory = is_memory_storage();

    // Links that run an API call from the page, act() is the script of /info
    auto action = [this, &tr](const std::string &path, const char *title, const char *confirm = nullptr) {
        return Fmt(R"html(<a href="/torrents/%s%s" onclick="return act(this.href, '%s')">%s</a>)html", hash().c_str(), path.c_str(),
                   confirm ? tr(confirm).c_str() : "", tr(title).c_str());
    };

    ss << "    " << label("label.name", 20) << Fmt("<b>%s</b> \n", html_escape(name()).c_str());
    ss << "    " << label("label.storage", 20) << tr(is_memory ? "storage.memory" : "storage.file") << " \n";
    ss << "    " << label("label.infohash", 20) << hash() << " \n";
    ss << "    " << label("label.status", 20) << tr(("state." + state_string()).c_str()) << " \n";
    ss << "    " << label("label.pieces_count", 20) << pieces_count() << " \n";
    ss << "    " << label("label.piece_length", 20) << humanize_bytes(piece_length()) << " \n";
    ss << "    " << label("label.players", 20) << players() << " \n";
    ss << "    " << label("label.readers", 20) << readers_count() << " \n";
    ss << "    " << label("label.actions", 20) << (is_paused() ? action("/resume", "action.resume") : action("/pause", "action.pause"));
    if (!is_memory)
        ss << " | " << action("/files/download", "action.download_all") << " | " << action("/files/stop", "action.stop_all");
    ss << " | " << action("/remove", "action.remove", "confirm.remove");
    if (!is_memory)
        ss << " | " << action("/remove?deleteData=true", "action.remove_with_data", "confirm.remove_with_data");
    ss << Fmt(R"( | <a href="/info?torrent=%s&lang=%s">%s</a>)", hash().c_str(), lang.c_str(), tr("action.only_this").c_str()) << "\n";
    if (auto meta = meta_info(); !meta.empty())
        ss << "    <details><summary>" << tr("label.meta_info") << "</summary>" << html_escape(meta) << "</details>\n";
    ss << "\n";
    ss << "    " << tr("section.speed") << ":\n";
    ss << "        " << label("label.download", 12) << humanize_bytes(status.download_payload_rate) << "/s \n";
    ss << "        " << label("label.upload", 12) << humanize_bytes(status.upload_payload_rate) << "/s \n";
    ss << "\n";
    ss << "    " << tr("section.size") << ":\n";
    ss << "        " << label("label.total", 24) << humanize_bytes(total()) << " \n";
    ss << "        " << label("label.done", 24) << Fmt("%s (%.2f%%) \n", humanize_bytes(status.total_done).c_str(), progress_total());
    ss << "        " << label("label.wanted", 24) << humanize_bytes(status.total_wanted) << " \n";
    ss << "        " << label("label.wanted_done", 24)
       << Fmt("%s (%.2f%%) \n", humanize_bytes(status.total_wanted_done).c_str(), progress_wanted());
    ss << "\n";
    ss << "    " << tr("section.connections") << ":\n";
    ss << "        " << label("label.connections", 24) << status.num_connections << " \n";
    ss << "        " << label("label.connections_limit", 24) << status.connections_limit << " \n";
    ss << "        " << label("label.connected_seeds", 24) << status.num_seeds << " \n";
    ss << "        " << label("label.connected_peers", 24) << status.num_peers << " \n";
    ss << "        " << label("label.all_seeds", 24) << total_seeds_count() << " \n";
    ss << "        " << label("label.all_peers", 24) << total_peers_count() << " \n";

    ss << "    " << tr("section.flags") << ":\n";
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

    if (is_files) {
        ss << "    " << tr("section.files") << ":\n";
        auto file_priorities = with_handle([](const lt::torrent_handle &h) { return h.get_file_priorities(); });
        auto progress = files_progress();
        for (auto &f : files()) {
            const int i = int(f->index());
            const int pr = std::size_t(i) < file_priorities.size() ? int(file_priorities[i]) : 0;
            const double pg = std::size_t(i) < progress.size() ? progress[i] * 100 : 0;
            auto file_action = [&](const char *path, const char *title) { return action(Fmt("/files/%d/%s", i, path), title); };

            ss << Fmt(R"(        <a href="%s" target=_blank>%s</a> (%s, %.2f%%, %s %d-%d))", f->stream_uri().c_str(),
                      html_escape(f->path()).c_str(), humanize_bytes(f->size()).c_str(), pg, tr("label.pieces").c_str(),
                      int(f->piece_start()), int(f->piece_end()))
               << "\n";
            ss << "            " << tr("label.priority") << ": " << (pr > 0 ? std::to_string(pr) : "-") << ", " << tr("label.readers")
               << ": " << readers_count(i);
            if (f->buffer_size() > 0)
                ss << ", " << tr("label.buffer") << Fmt(": %s (%.2f%%)", humanize_bytes(f->buffer_size()).c_str(), f->buffer_progress());
            ss << ", " << tr("label.actions") << ": ";
            if (!is_memory)
                ss << (pr > 0 ? file_action("stop", "action.stop") : file_action("download", "action.download")) << " | ";
            ss << file_action("buffer", "action.buffer");
            if (!is_memory) {
                ss << " | " << tr("label.priority")
                   << Fmt(R"html(: <select onchange="act('/torrents/%s/files/%d/set_priority?priority=' + this.value, '')">)html",
                          hash().c_str(), i);
                for (int p = int(lt::dont_download); p <= int(lt::top_priority); p++)
                    ss << Fmt("<option%s>%d</option>", p == pr ? " selected" : "", p);
                ss << "</select>";
            }
            ss << "\n";
        }
        ss << "\n";
    }

    if (is_trackers) {
        auto tracker_line = [&ss](const std::string &url, const std::string &tier, const std::string &state, const std::string &seeds,
                                  const std::string &peers, const std::string &downloaded, const std::string &announce,
                                  const std::string &message) {
            ss << "        " << pad_right(url, 61) << pad_right(tier, 5) << pad_right(state, 17) << pad_right(seeds, 7)
               << pad_right(peers, 7) << pad_right(downloaded, 11) << pad_right(announce, 10) << message << "\n";
        };

        ss << "    " << tr("section.trackers") << ":\n";
        tracker_line(tr("label.url"), tr("label.tier"), tr("label.status"), tr("label.seeds"), tr("label.peers"), tr("label.downloaded"),
                     tr("label.announce"), tr("label.message"));
        for (const auto &t : trackers_status()) {
            auto state = t.status == "failed" ? tr("tracker.failed", {std::to_string(t.fails)}) : tr(("tracker." + t.status).c_str());
            tracker_line(html_escape(t.url), t.tier < 0 ? "-" : std::to_string(t.tier), state, peer_num_to_string(t.seeds),
                         peer_num_to_string(t.peers), peer_num_to_string(t.downloaded),
                         t.next_announce >= 0 ? tr("tracker.next_announce", {std::to_string(t.next_announce)}) : "-",
                         html_escape(t.message));
        }
        ss << "\n";
    }

    if (is_pieces) {
        auto pieces = pieces_status();
        ss << "    " << tr("section.pieces") << " (" << tr("pieces.legend") << "):\n";
        for (std::size_t i = 0; i < pieces.map.size(); i += 100) {
            auto line = pieces.map.substr(i, 100);
            // Pieces read by streams in bold
            boost::replace_all(line, "*", "<b>*</b>");
            boost::replace_all(line, "?", "<b>?</b>");
            ss << "        " << line << "\n";
        }
        const int count = std::max(1, pieces.total);
        ss << "        "
           << tr("pieces.summary", {std::to_string(pieces.total), std::to_string(pieces.stored), Fmt("%.2f", 100. * pieces.stored / count),
                                    std::to_string(pieces.downloading), std::to_string(pieces.prioritized), std::to_string(pieces.readers)})
           << "\n";
    }
    ss << "\n\n";
    ss << "    " << std::string(150, '*') << "\n";
    ss << "\n\n";
}

void Torrent::save_resume_data() const {
    // Resume files are needed only for non-memory storage to keep the history and state.
    if (is_memory_storage())
        return;
    with_handle([this](const lt::torrent_handle &h) {
        h.save_resume_data();
        // Every request gets one save_resume_data_alert or save_resume_data_failed_alert
        m_pendingResumeData++;
    });
}

bool Torrent::has_pending_resume_data() const { return m_pendingResumeData > 0; }

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

int Torrent::readers_count(int file_index) const {
    std::scoped_lock guard(m_readersMutex);
    return int(std::count_if(m_readers.begin(), m_readers.end(),
                             [file_index](const auto &r) { return file_index < 0 || r.second->file_index() == file_index; }));
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

std::string Torrent::meta_info() const {
    std::scoped_lock guard(m_mutex);
    return m_metaInfo;
}

void Torrent::meta_info(const std::string &json) {
    std::scoped_lock guard(m_mutex);
    m_metaInfo = json;
    // Memory storage torrents are not kept between runs, their meta info neither
    if (!is_memory_storage() && !save_file(m_metaFile, m_metaInfo.data(), m_metaInfo.size()))
        throw lh::TorrentException("Could not save meta info to: " + m_metaFile);
}

int Torrent::players() const { return m_players; }

void Torrent::add_player() { m_players++; }

void Torrent::expire_players() {
    auto const now = std::chrono::steady_clock::now();
    auto const timeout = std::chrono::seconds(lh::config().player_timeout);
    // A crashed player's connections are closed by the system, so its readers are gone too
    if (m_players == 0 || has_readers() || timeout.count() <= 0) {
        m_playersIdleSince = now;
        return;
    }
    if (now - m_playersIdleSince < timeout)
        return;

    OATPP_LOGI("Torrent::expire_players", "Removing %d players of torrent %s, it has no stream connections for %d seconds", players(),
               hash().c_str(), int(timeout.count()));
    m_players = 0;
    m_playersIdleSince = now;
}

void Torrent::remove_player() {
    // Never below 0, when a client removes more players than it added
    int players = m_players;
    while (players > 0 && !m_players.compare_exchange_weak(players, players - 1)) {
    }
}

} // namespace lh
