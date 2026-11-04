#pragma once

#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_status.hpp>

namespace lh {

// Same values and names as Elementum's torrent states, clients may compare the numbers
enum class TorrentState {
    Queued = 0,
    Checking,
    Finding,
    Downloading,
    Finished,
    Seeding,
    Allocating, // Not used by libtorrent 1.2, kept for Elementum's numbering
    Stalled,
    Paused,
    Buffering,
    Playing,
    Error
};

inline const char *state_to_string(TorrentState state) {
    switch (state) {
    case TorrentState::Queued:
        return "Queued";
    case TorrentState::Checking:
        return "Checking";
    case TorrentState::Finding:
        return "Finding";
    case TorrentState::Downloading:
        return "Downloading";
    case TorrentState::Finished:
        return "Finished";
    case TorrentState::Seeding:
        return "Seeding";
    case TorrentState::Allocating:
        return "Allocating";
    case TorrentState::Stalled:
        return "Stalled";
    case TorrentState::Paused:
        return "Paused";
    case TorrentState::Buffering:
        return "Buffering";
    case TorrentState::Playing:
        return "Playing";
    case TorrentState::Error:
        return "Error";
    }
    return "Unknown";
}

// is_paused: paused with the API, is_buffering: a file buffers, players: players registered with the API
inline TorrentState compute_state(const lt::torrent_status &status, bool has_metadata, bool is_paused, bool is_buffering, int players) {
    const bool is_complete = status.total_wanted > 0 && status.total_wanted_done >= status.total_wanted;
    // Paused by libtorrent's queue, not by us
    const bool is_queued = (status.flags & lt::torrent_flags::auto_managed) && (status.flags & lt::torrent_flags::paused);

    if (status.errc)
        return TorrentState::Error;
    if (is_paused)
        return is_complete ? TorrentState::Finished : TorrentState::Paused;
    if (status.state == lt::torrent_status::checking_files || status.state == lt::torrent_status::checking_resume_data)
        return TorrentState::Checking;
    if (is_queued)
        return TorrentState::Queued;
    if (!has_metadata)
        return TorrentState::Finding;
    if (is_buffering)
        return TorrentState::Buffering;
    if (players > 0)
        return TorrentState::Playing;
    if (status.is_seeding)
        return TorrentState::Seeding;
    if (is_complete || status.state == lt::torrent_status::finished || status.total_wanted == 0)
        return TorrentState::Finished;
    if (status.download_payload_rate == 0)
        return TorrentState::Stalled;
    return TorrentState::Downloading;
}

} // namespace lh
