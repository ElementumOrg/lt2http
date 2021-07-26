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

#include <utils/date.h>
#include <utils/numbers.h>
#include <utils/path.h>
#include <utils/strings.h>

using namespace date;
using namespace std::chrono;

namespace lh {

Torrent::Torrent(std::shared_ptr<lt::session> nativeSession, const lt::torrent_handle &nativeHandle, const lh::storage_type_t st, std::chrono::time_point<std::chrono::system_clock> added_time)
    : m_nativeSession(std::move(nativeSession))
    , m_nativeHandle(nativeHandle)
    , m_nativeInfo(std::const_pointer_cast<lt::torrent_info>(nativeHandle.torrent_file()))
    , m_hash(to_hex(m_nativeHandle.info_hash()))
    , m_storageType(st)
    , m_addedTime(added_time) {
    auto &config = lh::config();

    m_torrentFile = torrent_file(config.torrents_path, m_hash);
    m_resumeFile = resume_file(config.torrents_path, m_hash);
    m_partsFile = parts_file(config.download_path, m_hash);

    OATPP_LOGI("Torrent", "Creating torrent with hash: %s", hash().c_str());
}

Torrent::~Torrent() { OATPP_LOGI("Torrent", "Destroying: %s", hash().c_str()); }

lt::torrent_status &Torrent::status() { return m_nativeStatus; };

std::vector<std::shared_ptr<File>> Torrent::files() { return m_files; };

std::shared_ptr<File> Torrent::get_file(int index) {
    auto match =
        std::find_if(m_files.begin(), m_files.end(), [&index](const std::shared_ptr<File> &t) { return t->index() == index; });
    if (match == m_files.end())
        throw lh::FileException(Fmt("File with index '%d' not found in torrent with hash '%s'", index, hash().c_str()));

    return *match;
};

lt::storage_interface* Torrent::storage() {
    if (is_memory_storage())
        return m_memory_storage;

    return m_file_storage;
};

bool Torrent::is_closing() const { return m_isClosing; }

bool Torrent::is_valid() const { return (m_nativeInfo && m_nativeInfo->is_valid() && (m_nativeInfo->num_files() > 0)); }

std::string Torrent::name() const {
    if (!m_name.empty())
        return m_name;

    if (has_metadata())
        return m_nativeInfo->name();

    if (!m_nativeStatus.name.empty())
        return m_nativeStatus.name;

    return "";
}

std::string Torrent::hash() const { return m_hash; }

std::int64_t Torrent::size() const {
    if (!is_valid())
        return -1;
    return m_nativeInfo->total_size();
}

int Torrent::files_count() const {
    if (!is_valid())
        return -1;
    return m_nativeInfo->num_files();
}

int Torrent::piece_length() const {
    if (!is_valid())
        return -1;
    return m_nativeInfo->piece_length();
}

int Torrent::piece_length_at(const int index) const {
    if (!is_valid())
        return -1;
    return m_nativeInfo->piece_size(lt::piece_index_t{index});
}

int Torrent::pieces_count() const {
    if (!is_valid())
        return -1;
    return m_nativeInfo->num_pieces();
}

bool Torrent::have_piece(lt::piece_index_t piece) const { return m_nativeHandle.have_piece(piece); };

bool Torrent::has_metadata() const { return (m_nativeInfo && m_nativeInfo->is_valid() && (m_nativeInfo->num_files() > 0)); }

void Torrent::update_metadata() {
    m_nativeInfo = std::const_pointer_cast<lt::torrent_info>(m_nativeHandle.torrent_file());

    on_metadata_received();
}

void Torrent::on_metadata_received() {
    save_torrent_file();
    save_resume_data();

    if (is_memory_storage()) {
        m_memory_storage = dynamic_cast<lh::memory_storage *>(m_nativeHandle.get_storage_impl());
        m_memory_storage->set_torrent_handle(m_nativeHandle);
    } else {
        m_file_storage = m_nativeHandle.get_storage_impl();
    }

    m_deadline_pieces.resize(m_nativeInfo->num_pieces());
    auto files = m_nativeInfo->files();

    m_files.clear();
    for (int i = 0; i < m_nativeInfo->num_files(); i++) {
        auto name = std::string(files.file_name(i));
        auto path = std::string(files.file_path(i));
        m_files.emplace_back(std::make_shared<lh::File>(this, lt::file_index_t(i), std::string(files.file_path(i)),
                                               std::string(files.file_name(i)), files.file_size(i), files.file_offset(i),
                                               file_priority(i)));
    }
}

void Torrent::remove(bool is_delete_files, bool is_delete_data) {
    m_isClosing = true;

    auto flags = is_delete_data && !is_memory_storage() ? lt::session::delete_files : lt::session::delete_partfile;
    m_nativeSession->remove_torrent(m_nativeHandle, flags);

    if (is_delete_files) {
        // Remove torrent file
        if (file_exists(m_torrentFile)) {
            OATPP_LOGI("Torrent::remove", "Removing file at: %s", m_torrentFile.c_str())
            remove_file(m_torrentFile);
        }
    }

    if (is_delete_files || is_memory_storage()) {
        // Remove resume + parts file
        if (file_exists(m_resumeFile)) {
            OATPP_LOGI("Torrent::remove", "Removing file at: %s", m_resumeFile.c_str())
            remove_file(m_resumeFile);
        }

        if (file_exists(m_partsFile)) {
            OATPP_LOGI("Torrent::remove", "Removing file at: %s", m_partsFile.c_str())
            remove_file(m_partsFile);
        }
    }

    OATPP_LOGI("Torrent::remove", "Removed torrent with hash: %s", hash().c_str());
}

bool Torrent::is_memory_storage() const { return m_storageType == storage_type_t::memory; }

void Torrent::update_status() { update_status(m_nativeHandle.status()); }

void Torrent::update_status(const lt::torrent_status &nativeStatus) {
    m_nativeStatus = nativeStatus;
    update_state();
}

TorrentState Torrent::state() const { return m_state; }

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

void Torrent::update_state() {
    if (m_nativeStatus.state == lt::torrent_status::checking_resume_data) {
        m_state = TorrentState::CheckingResumeData;
    }
    // else if (isMoveInProgress())
    // {
    //     m_state = TorrentState::Moving;
    // }
    // else if (hasMissingFiles())
    // {
    //     m_state = TorrentState::MissingFiles;
    // }
    else if (has_error()) {
        m_state = TorrentState::Error;
    } else if (!has_metadata()) {
        if (is_paused())
            m_state = TorrentState::PausedDownloading;
        // else if (m_session->isQueueingSystemEnabled() && isQueued())
        else if (is_queued())
            m_state = TorrentState::QueuedDownloading;
        else
            m_state = TorrentState::DownloadingMetadata;
    } else if ((m_nativeStatus.state == lt::torrent_status::checking_files) &&
               (!is_paused() || (m_nativeStatus.flags & lt::torrent_flags::auto_managed) ||
                !(m_nativeStatus.flags & lt::torrent_flags::paused))) {
        // If the torrent is not just in the "checking" state, but is being actually checked
        m_state = m_hasSeedStatus ? TorrentState::CheckingUploading : TorrentState::CheckingDownloading;
    } else if (m_nativeStatus.total_wanted == 0) {
        m_state = TorrentState::Queued;
    } else if (is_seed()) {
        if (is_paused())
            m_state = TorrentState::PausedUploading;
        // else if (m_session->isQueueingSystemEnabled() && isQueued())
        //     m_state = TorrentState::QueuedUploading;
        else if (is_forced())
            m_state = TorrentState::ForcedUploading;
        else if (m_nativeStatus.upload_payload_rate > 0)
            m_state = TorrentState::Uploading;
        else
            m_state = TorrentState::StalledUploading;
    } else {
        if (is_paused())
            m_state = TorrentState::PausedDownloading;
        // else if (m_session->isQueueingSystemEnabled() && isQueued())
        //     m_state = TorrentState::QueuedDownloading;
        else if (is_forced())
            m_state = TorrentState::ForcedDownloading;
        else if (m_nativeStatus.download_payload_rate > 0)
            m_state = TorrentState::Downloading;
        else
            m_state = TorrentState::StalledDownloading;
    }
}

bool Torrent::has_error() const { return static_cast<bool>(m_nativeStatus.errc); }

bool Torrent::is_buffering() const {
    auto match = std::find_if(m_files.begin(), m_files.end(),
                              [](const std::shared_ptr<File> &f) { return f->is_buffering() && f->buffer_progress() < 100; });
    return match != m_files.end();
}

bool Torrent::is_paused() const { return m_isStopped; }

bool Torrent::is_queued() const {
    // Torrent is Queued if it isn't in Paused state but paused internally
    return (!is_paused() && (m_nativeStatus.flags & lt::torrent_flags::auto_managed) &&
            (m_nativeStatus.flags & lt::torrent_flags::paused));
}

bool Torrent::is_checking() const {
    return ((m_nativeStatus.state == lt::torrent_status::checking_files) ||
            (m_nativeStatus.state == lt::torrent_status::checking_resume_data));
}

bool Torrent::is_downloading() const {
    return m_state == TorrentState::Downloading || m_state == TorrentState::DownloadingMetadata ||
           m_state == TorrentState::StalledDownloading || m_state == TorrentState::CheckingDownloading ||
           m_state == TorrentState::PausedDownloading || m_state == TorrentState::QueuedDownloading ||
           m_state == TorrentState::ForcedDownloading;
}

bool Torrent::is_uploading() const {
    return m_state == TorrentState::Uploading || m_state == TorrentState::StalledUploading ||
           m_state == TorrentState::CheckingUploading || m_state == TorrentState::QueuedUploading ||
           m_state == TorrentState::ForcedUploading;
}

bool Torrent::is_completed() const {
    return m_state == TorrentState::Uploading || m_state == TorrentState::StalledUploading ||
           m_state == TorrentState::CheckingUploading || m_state == TorrentState::PausedUploading ||
           m_state == TorrentState::QueuedUploading || m_state == TorrentState::ForcedUploading;
}

bool Torrent::is_active() const {
    if (m_state == TorrentState::StalledDownloading)
        return (upload_rate() > 0);

    return m_state == TorrentState::DownloadingMetadata || m_state == TorrentState::Downloading ||
           m_state == TorrentState::ForcedDownloading || m_state == TorrentState::Uploading ||
           m_state == TorrentState::ForcedUploading || m_state == TorrentState::Moving;
}

bool Torrent::is_inactive() const { return !is_active(); }

bool Torrent::is_errored() const { return m_state == TorrentState::MissingFiles || m_state == TorrentState::Error; }

bool Torrent::is_seed() const {
    return ((m_nativeStatus.state == lt::torrent_status::finished) || (m_nativeStatus.state == lt::torrent_status::seeding));
}

bool Torrent::is_forced() const { return !is_paused(); }

bool Torrent::is_sequential_download() const {
    return static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::sequential_download);
}

std::int64_t Torrent::upload_rate() const { return m_nativeStatus.upload_payload_rate; }

std::int64_t Torrent::download_rate() const { return m_nativeStatus.download_payload_rate; }

std::int64_t Torrent::total() const {
    if (!is_valid())
        return -1;
    return m_nativeInfo->total_size();
}

std::int64_t Torrent::total_done() const { return m_nativeStatus.total_done; }

std::int64_t Torrent::total_wanted() const { return m_nativeStatus.total_wanted; }

std::int64_t Torrent::total_wanted_done() const { return m_nativeStatus.total_wanted_done; }

std::int64_t Torrent::total_payload_upload() const { return m_nativeStatus.total_payload_upload; }

std::int64_t Torrent::total_payload_download() const { return m_nativeStatus.total_payload_download; }

std::int64_t Torrent::total_download() const { return m_nativeStatus.all_time_download; }

std::int64_t Torrent::total_upload() const { return m_nativeStatus.all_time_upload; }

std::int64_t Torrent::active_time() const { return lt::total_seconds(m_nativeStatus.active_duration); }

std::int64_t Torrent::finished_time() const { return lt::total_seconds(m_nativeStatus.finished_duration); }

std::int64_t Torrent::seeding_time() const { return lt::total_seconds(m_nativeStatus.seeding_duration); }

int Torrent::connections_count() const { return m_nativeStatus.num_connections; }

int Torrent::connections_limit() const { return m_nativeStatus.connections_limit; }

int Torrent::seeds_count() const { return m_nativeStatus.num_seeds; }

int Torrent::peers_count() const { return m_nativeStatus.num_peers; }

int Torrent::leechers_count() const { return (m_nativeStatus.num_peers - m_nativeStatus.num_seeds); }

int Torrent::total_seeds_count() const {
    return (m_nativeStatus.num_complete > 0) ? m_nativeStatus.num_complete : m_nativeStatus.list_seeds;
}

int Torrent::total_peers_count() const {
    const int peers = m_nativeStatus.num_complete + m_nativeStatus.num_incomplete;
    return (peers > 0) ? peers : m_nativeStatus.list_peers;
}

int Torrent::total_leechers_count() const {
    return (m_nativeStatus.num_incomplete > 0) ? m_nativeStatus.num_incomplete
                                               : (m_nativeStatus.list_peers - m_nativeStatus.list_seeds);
}

int Torrent::complete_count() const { return m_nativeStatus.num_complete; }

int Torrent::incomplete_count() const {
    return m_nativeStatus.num_incomplete;
}

double Torrent::progress() const {
    if (is_checking())
        return m_nativeStatus.progress;

    if (m_nativeStatus.total_wanted == 0)
        return 0.;

    if (m_nativeStatus.total_wanted_done == m_nativeStatus.total_wanted)
        return 1.;

    const double progress = static_cast<double>(m_nativeStatus.total_wanted_done) / m_nativeStatus.total_wanted;
    return progress;
}

double Torrent::progress_total() const {
    if (total() <= 0)
        return 0;

    return 100 * (double(total_done()) / double(total()));
}

double Torrent::progress_wanted() const {
    if (total_wanted() <= 0)
        return 0;

    return 100 * (double(total_wanted_done()) / double(total_wanted()));
}

std::vector<double> Torrent::files_progress() const {
    if (!has_metadata())
        return {};

    std::vector<int64_t> fp;
    m_nativeHandle.file_progress(fp, lt::torrent_handle::piece_granularity);

    const int count = static_cast<int>(fp.size());
    std::vector<double> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        const std::int64_t size = m_files[i]->size();
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

    std::vector<int64_t> fp;
    m_nativeHandle.file_progress(fp, lt::torrent_handle::piece_granularity);

    const int count = static_cast<int>(fp.size());
    std::vector<std::int64_t> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(fp[i]);
    }

    return result;
}

lt::download_priority_t Torrent::file_priority(lt::file_index_t index) const { return m_nativeHandle.file_priority(index); };

void Torrent::file_priority(lt::file_index_t index, lt::download_priority_t priority) {
    if (is_memory_storage())
        return;

    OATPP_LOGI("Torrent::file_priority", "Setting priority '%d' for file '%d'", priority, index);
    m_nativeHandle.file_priority(index, priority);
    save_resume_data();
};

lt::download_priority_t Torrent::piece_priority(lt::piece_index_t index) const { return m_nativeHandle.piece_priority(index); };

void Torrent::piece_priority(lt::piece_index_t index, lt::download_priority_t priority) {
    OATPP_LOGI("Torrent::piece_priority", "Setting priority '%d' for piece '%d'", priority, index);
    if (priority < lt::dont_download)
        priority = lt::dont_download;
    else if (priority > lt::top_priority)
        priority = lt::top_priority;

    m_nativeHandle.piece_priority(index, priority);
};

void Torrent::set_piece_deadline(lt::piece_index_t index, int deadline) {
    if (m_deadline_pieces.test(index)) {
        OATPP_LOGI("Torrent::set_piece_deadline", "Skipping deadline for piece '%d'", index);
        return;
    }

    OATPP_LOGI("Torrent::set_piece_deadline", "Setting deadline '%d' for piece '%d'", deadline, index);
    m_deadline_pieces.set(index);
    m_nativeHandle.set_piece_deadline(index, deadline);
};

void Torrent::clear_piece_deadline(lt::piece_index_t index) {
    m_deadline_pieces.reset(index);
};

void Torrent::prioritize_pieces(lt::piece_index_t start, lt::piece_index_t end) {
    for (int i = start; i <= end; ++i) {
        if (m_nativeHandle.have_piece(i))
            continue;

        if (i == int(start)) {
            set_piece_priority(i, 0, lt::download_priority_t{7});
        } else {
            set_piece_priority(i, (i - int(start)) * 10, lt::download_priority_t{6});
        }
    }
};

void Torrent::set_piece_priority(int piece, int deadline, lt::download_priority_t priority) {
    if (piece_priority(piece) < priority) {
        piece_priority(piece, priority);
        set_piece_deadline(piece, deadline);
    }
}

void Torrent::dispatch_alert(const lt::alert *a) {
    switch (a->type()) {
    case lt::save_resume_data_alert::alert_type:
        handle_save_resume_data_alert(static_cast<const lt::save_resume_data_alert *>(a));
        break;
    case lt::dht_reply_alert::alert_type:
        handle_dht_reply_alert(static_cast<const lt::dht_reply_alert *>(a));
        break;
    case lt::tracker_reply_alert::alert_type:
        handle_tracker_reply_alert(static_cast<const lt::tracker_reply_alert *>(a));
        break;
    case lt::tracker_warning_alert::alert_type:
        handle_tracker_warning_alert(static_cast<const lt::tracker_warning_alert *>(a));
        break;
    case lt::tracker_error_alert::alert_type:
        handle_tracker_error_alert(static_cast<const lt::tracker_error_alert *>(a));
        break;
    }
}

void Torrent::handle_save_resume_data_alert(const lt::save_resume_data_alert *p) {
    OATPP_LOGI("Torrent::handle_save_resume_data_alert", "Saving resume data to: %s", m_resumeFile.c_str());

    std::ofstream of(m_resumeFile, std::ios_base::binary);
    of.unsetf(std::ios_base::skipws);
    auto const b = lt::write_resume_data_buf(p->params);
    of.write(b.data(), b.size());
};

void Torrent::handle_dht_reply_alert(const lt::dht_reply_alert *p) { 
    update_tracker("DHT", "", p->num_peers); 
};

void Torrent::handle_tracker_reply_alert(const lt::tracker_reply_alert *p) {
    update_tracker(p->tracker_url(), p->message(), p->num_peers);
};
void Torrent::handle_tracker_warning_alert(const lt::tracker_warning_alert *p) {
    update_tracker(p->tracker_url(), p->warning_message(), 0);
};

void Torrent::handle_tracker_error_alert(const lt::tracker_error_alert *p) {
    update_tracker(p->tracker_url(), p->error_message(), 0);
};

void Torrent::set_auto_managed(const bool enable) {
    if (enable)
        m_nativeHandle.set_flags(lt::torrent_flags::auto_managed);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::auto_managed);
}

void Torrent::pause() {
    if (!m_isStopped)
        m_isStopped = true;

    set_auto_managed(false);
    m_nativeHandle.pause();
}

void Torrent::resume() {
    if (has_error())
        m_nativeHandle.clear_error();

    if (m_isStopped) {
        // Torrent may have been temporarily resumed to perform checking files
        // so we have to ensure it will not pause after checking is done.
        m_nativeHandle.unset_flags(lt::torrent_flags::stop_when_ready);
        m_isStopped = false;
    }

    set_auto_managed(true);
    m_nativeHandle.resume();
}

void Torrent::update_tracker(const std::string &name, std::string message, int number) {
    auto it = m_trackerInfos.find(name);
    if (it == m_trackerInfos.end())
        m_trackerInfos[name] = {};

    m_trackerInfos[name].lastMessage = std::move(message);
    m_trackerInfos[name].numPeers = std::max(number, m_trackerInfos[name].numPeers);
}

void Torrent::dump(std::stringstream &ss) {
    ss << Fmt("    Name:               %s \n", name().c_str());
    ss << Fmt("    Storage:            %s \n", storage_to_string(m_storageType));
    ss << Fmt("    Infohash:           %s \n", hash().c_str());
    ss << Fmt("    Status:             %s \n", state_string().c_str());
    ss << Fmt("    Pieces count:       %d \n", pieces_count());
    ss << Fmt("    Piece length:       %s \n", humanize_bytes(piece_length()).c_str());
    ss << "\n";
    ss << "    Speed:\n";
    ss << Fmt("        Download:   %s/s \n", humanize_bytes(download_rate()).c_str());
    ss << Fmt("        Upload:     %s/s \n", humanize_bytes(upload_rate()).c_str());
    ss << "\n";
    ss << "    Size:\n";
    ss << Fmt("        Total:                  %s \n", humanize_bytes(total()).c_str());
    ss << Fmt("        Done:                   %s (%.2f%%) \n", humanize_bytes(total_done()).c_str(), progress_total());
    ss << Fmt("        Wanted:                 %s \n", humanize_bytes(total_wanted()).c_str());
    ss << Fmt("        Wanted done:            %s (%.2f%%) \n", humanize_bytes(total_wanted_done()).c_str(), progress_wanted());
    ss << "\n";
    ss << "    Connections:\n";
    ss << Fmt("        Connections:            %d \n", connections_count());
    ss << Fmt("        Connections limit:      %d \n", connections_limit());
    ss << Fmt("        Connected seeds:        %d \n", seeds_count());
    ss << Fmt("        Connected peers:        %d \n", peers_count());
    ss << Fmt("        All Seeds:              %d \n", total_seeds_count());
    ss << Fmt("        All Peers:              %d \n", total_peers_count());

    ss << "    Flags:\n";
    ss << Fmt("        paused: %s \n", static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::paused) ? "true" : "false");
    ss << Fmt("        auto_managed: %s \n",
              static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::auto_managed) ? "true" : "false");
    ss << Fmt("        sequential_download: %s \n",
              static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::sequential_download) ? "true" : "false");
    ss << Fmt("        need_save_resume: %s \n",
              static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::need_save_resume) ? "true" : "false");
    ss << Fmt("        is_seeding: %s \n", m_nativeStatus.is_seeding ? "true" : "false");
    ss << Fmt("        is_finished: %s \n", m_nativeStatus.is_finished ? "true" : "false");
    ss << Fmt("        has_metadata: %s \n", m_nativeStatus.has_metadata ? "true" : "false");
    ss << Fmt("        has_incoming: %s \n", m_nativeStatus.has_incoming ? "true" : "false");
    ss << Fmt("        moving_storage: %s \n", m_nativeStatus.moving_storage ? "true" : "false");
    ss << Fmt("        announcing_to_trackers: %s \n", m_nativeStatus.announcing_to_trackers ? "true" : "false");
    ss << Fmt("        announcing_to_lsd: %s \n", m_nativeStatus.announcing_to_lsd ? "true" : "false");
    ss << Fmt("        announcing_to_dht: %s \n", m_nativeStatus.announcing_to_dht ? "true" : "false");
    ss << "\n";

    ss << "    Files (Priority):\n";
    auto priorities = m_nativeHandle.get_file_priorities();
    for (auto &f : m_files) {
        auto pr = priorities[f->index()];
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

    std::map<std::string, std::string> trackers;
    for (auto &tracker : m_nativeHandle.trackers()) {
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

        if (scrape_incomplete == -1) {
            auto it = m_trackerInfos.find(tracker.url);
            if (it != m_trackerInfos.end()) {
                scrape_incomplete = it->second.numPeers;
            }
        }

        trackers.insert(std::pair<std::string, std::string>(tracker.url, Fmt("        %-60s: %-3s seeds, %-3s peers, updating: %-5s, is_working: %-5s, message: %s\n", tracker.url.c_str(),
                   peer_num_to_string(scrape_complete).c_str(), peer_num_to_string(scrape_incomplete).c_str(),
                   is_updating ? "true" : "false", is_working ? "true" : "false", message.c_str())));
    }

    // List trackers information sorted by tracker url.
    for (auto &i : trackers) {
        ss << i.second;
    }

    if (m_trackerInfos.find("DHT") != m_trackerInfos.end()) {
        auto entry = m_trackerInfos["DHT"];
        ss << Fmt("        %-60s: %-3s seeds, %-3s peers, updating: %-5s, is_working: %-5s, message: %s\n", "DHT", "-",
                  peer_num_to_string(entry.numPeers).c_str(), "false", "true", entry.lastMessage.c_str());
    }
    ss << "\n";

    ss << "    Pieces:\n";
    ss << "        ";

    std::vector<int> reader_positions;
    for (const auto& reader : m_readers) {
        reader_positions.push_back(reader.second->piece_start());
    }

    int total_prioritized = 0;
    int total_have = 0;
    for (int i = 0; i < pieces_count(); i++) {
        if (std::find(reader_positions.begin(), reader_positions.end(), i) != reader_positions.end()) {
            total_prioritized++;
            // Piece is in scope of Readers
            if (m_nativeHandle.have_piece(lt::piece_index_t{i})) {
                total_have++;
                ss << "<b>*</b>";
            } else {
                ss << "<b>?</b>";
            }
        } else if (m_nativeHandle.have_piece(lt::piece_index_t{i})) {
            // We already have this piece
            total_have++;
            if (m_nativeHandle.piece_priority(lt::piece_index_t{i}) > 0)
                total_prioritized++;

            ss << "+";
        } else if (m_nativeHandle.piece_priority(lt::piece_index_t{i}) == 0) {
            // Piece is not prioritized
            ss << "-";
        } else {
            // Piece is in the queue
            total_prioritized++;
            ss << int(m_nativeHandle.piece_priority(lt::piece_index_t{i}));
        }

        if (i > 0 && (i + 1) % 100 == 0) {
            ss << "\n        ";
        }
    }
    ss << Fmt("\n        Readers: <b>%d</b>, Stored: <b>%d</b>, Prioritized: <b>%d</b>",
                m_readers.size(), total_have, total_prioritized);
    ss << "\n\n\n";
    ss << "    " << std::string(150, '*') << "\n";
    ss << "\n\n";
}

void Torrent::save_resume_data() const {
    // Resume files are needed only for non-memory storage to keep the history and state.
    if (!is_memory_storage())
        m_nativeHandle.save_resume_data();
}

void Torrent::save_torrent_file() const {
    if (!has_metadata() || is_memory_storage() || file_exists(m_torrentFile))
        return;

    OATPP_LOGI("Torrent::save_torrent_file", "Saving torrent file to: %s", m_torrentFile.c_str());

    const lt::create_torrent creator = lt::create_torrent(*m_nativeInfo);
    const lt::entry e = creator.generate();

    std::vector<char> ret;
    lt::bencode(std::back_inserter(ret), e);

    std::ofstream of(m_torrentFile, std::ios_base::binary);
    of.unsetf(std::ios_base::skipws);
    of.write(ret.data(), ret.size());
};

std::vector<char> Torrent::generate() const {
    if (!has_metadata())
        throw lh::TorrentException("Torrent does not have metadata");

    OATPP_LOGI("Torrent::generate", "Generating torrent file for: %s", hash().c_str());

    const lt::create_torrent creator = lt::create_torrent(*m_nativeInfo);
    const lt::entry e = creator.generate();

    std::vector<char> ret;
    lt::bencode(std::back_inserter(ret), e);
    return ret;
}

std::int64_t Torrent::memory_size() const {
    if (is_memory_storage())
        return m_memory_storage->get_memory_size();

    return lh::session().memory_size();
};

void Torrent::memory_size(std::int64_t size) {
    if (is_memory_storage())
        m_memory_storage->set_memory_size(size);
    else
        lh::config().memory_size = int(size / 1024 / 1024);
};

int Torrent::readahead_pieces() const {
    if (is_memory_storage())
        return m_memory_storage->get_buffers_count() * lh::config().readahead_percents / 100;
    
    // That is a static value of pieces with priority > 1 to set for file storage.
    return lh::file_readahead_pieces;
};

bool Torrent::has_readers() const { return !m_readers.empty(); };

void Torrent::register_reader(std::int64_t id, Reader* reader) { 
    m_readers[id] = reader;
}

void Torrent::unregister_reader(const std::int64_t &id) {
    if (m_readers.find(id) != m_readers.end()) {
        m_readers.erase(id);
    }
}

std::map<std::int64_t, Reader*> Torrent::readers() const { return m_readers; };

void Torrent::prioritize() {
    int pieces_limit = readahead_pieces();
    int iter_count = -1;
    int readers_finished = 0;
    std::vector<int> reader_pieces;

    for (const auto &p : m_readers) {
        p.second->set_iterated(false);
    }

    while (readers_finished < m_readers.size() && pieces_limit > 0) {
        iter_count++;
        for (const auto &p : m_readers) {
            int index = p.second->piece_start() + iter_count;
            if (p.second->is_iterated()) {
                continue;
            }
            if (p.second->is_closing() || index > p.second->piece_end_limit()) {
                p.second->set_iterated(true);
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

    // Remove non-needed priorities to release place for new pieces
    std::vector<std::pair<lt::piece_index_t, lt::download_priority_t>> pieces_request;
    if (is_memory_storage()) {
        auto priorities = m_nativeHandle.get_piece_priorities();
        for (std::size_t piece = 0; piece < priorities.size(); ++piece) {
            if (priorities.at(piece) <= 0)
                continue;
            if (std::find(reader_pieces.begin(), reader_pieces.end(), piece) == reader_pieces.end()) {
                pieces_request.emplace_back(std::pair<lt::piece_index_t, lt::download_priority_t>{piece, 0});
            }
        }
    }

    // Prioritize readehead pieces with priority groups, each next group should be bigger than previous,
    //  to have proper prioritization.
    int progression_priority = 5;
    int progression_start = 2;
    double progression_multiplier = 1.7;
    int progression_group = 1;
    int progression_left = progression_result(progression_start, progression_multiplier, progression_group);

    for (const auto& piece : reader_pieces) {
        if (progression_left <= 0 && progression_priority > 1) {
            progression_priority--;
            progression_group++;
            progression_left = progression_result(progression_start, progression_multiplier, progression_group);
        }

        progression_left--;
        int priority = std::max(2, progression_priority);
        if (!have_piece(piece) && (piece_priority(piece) <= 0 || int(piece_priority(piece)) < priority)) {
            pieces_request.emplace_back(std::pair<lt::piece_index_t, lt::download_priority_t>{piece, priority});
        }
    }

    if (!pieces_request.empty()) {
        OATPP_LOGI("Torrent::prioritize", "Prioritizing %d pieces out of %d possible", pieces_request.size(), readahead_pieces())
        m_nativeHandle.prioritize_pieces(pieces_request);
    }
    if (is_memory_storage()) {
        m_memory_storage->update_reader_pieces(reader_pieces);
    }
};

void Torrent::update_buffer_progress() {
    if (!is_buffering())
        return;

    std::vector<lt::partial_piece_info> pieces;
    m_nativeHandle.get_download_queue(pieces);

    for (const auto& file : m_files) {
        if (!file->is_buffering() || file->buffer_progress() >= 100 || file->buffer_size() <= 0)
            continue;

        auto buffer_pieces = file->buffer_pieces();
        auto buffer_size = file->buffer_size();
        std::int64_t missing = 0;

        for (const auto& piece : buffer_pieces) {
            if (!have_piece(piece))
                missing += piece_length_at(piece);
        }

        if (missing > 0) {
            for (const auto& queue : pieces) {
                if (std::find(buffer_pieces.begin(), buffer_pieces.end(), queue.piece_index) != buffer_pieces.end()) {
                    auto *blocks = queue.blocks;
                    auto blocksInPiece = queue.blocks_in_piece;
                    for (int b = 0; b < blocksInPiece; b++) {
                        missing -= blocks[b].bytes_progress;
                    }
                }
            }
        }

        file->set_buffer_progress(double(buffer_size - missing) / buffer_size * 100);
    }
}

} // namespace lh
