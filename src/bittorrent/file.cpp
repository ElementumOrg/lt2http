#include "file.h"

#include <libtorrent/download_priority.hpp>

#include <oatpp/core/Types.hpp>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include <app/application.h>

#include <bittorrent/reader.h>
#include <bittorrent/torrent.h>

#include <utils/pieces.h>
#include <utils/strings.h>

namespace lh {

File::File(Torrent* torrent, lt::file_index_t index, std::string file_path, std::string file_name,
           std::int64_t file_size, std::int64_t file_offset, lt::download_priority_t priority)
    : m_torrent(torrent), m_index(index), m_path(std::move(file_path)), m_name(std::move(file_name)),
      m_size(file_size), m_offset(file_offset) {
    auto const &config = lh::config();

    m_start_buffer_size = std::int64_t(config.buffer_size) * 1024 * 1024;
    m_end_buffer_size = std::int64_t(config.end_buffer_size) * 1024 * 1024;

    std::tie(m_piece_start, m_piece_end) = region_pieces(m_offset, m_size, m_torrent->piece_length(), m_torrent->pieces_count());
}

File::~File() = default;

lt::file_index_t File::index() const { return m_index; }

const std::string &File::path() const { return m_path; }

const std::string &File::name() const { return m_name; }

std::int64_t File::size() const { return m_size; }

std::int64_t File::offset() const { return m_offset; }

std::string File::stream_uri() const {
    return Fmt("/torrents/%s/files/%d/stream/%s", m_torrent->hash().c_str(), int(m_index), uri_escape(m_name).c_str());
}

lt::piece_index_t File::piece_start() const { return m_piece_start; }

lt::piece_index_t File::piece_end() const { return m_piece_end; }

lt::download_priority_t File::priority() const { return m_torrent->file_priority(index()); }

void File::set_priority(lt::download_priority_t priority) {
    if (!m_torrent->is_memory_storage())
        m_torrent->file_priority(m_index, priority);
}

void File::start_buffer() {
    OATPP_LOGI("File::start_buffer", "Buffering file: %s/%s", m_torrent->hash().c_str(), m_name.c_str())

    std::vector<int> pieces;
    std::int64_t size = 0;
    auto add_region = [&](std::int64_t offset, std::int64_t length) {
        auto [first, last] = region_pieces(offset, length, m_torrent->piece_length(), m_torrent->pieces_count());
        for (int piece = int(first); piece <= int(last); piece++) {
            if (std::find(pieces.begin(), pieces.end(), piece) != pieces.end())
                continue;
            pieces.push_back(piece);
            size += m_torrent->piece_length_at(piece);
        }
    };

    if (m_size >= m_start_buffer_size + m_end_buffer_size) {
        add_region(m_offset, m_start_buffer_size);
        add_region(m_offset + m_size - m_end_buffer_size, m_end_buffer_size);
    } else {
        add_region(m_offset, m_size);
    }

    for (int piece : pieces) {
        m_torrent->piece_priority(piece, lt::top_priority);
        m_torrent->set_piece_deadline(piece, 0);
    }

    if (m_torrent->is_memory_storage() && !pieces.empty()) {
        if (size > m_torrent->memory_size()) {
            OATPP_LOGI("File::start_buffer", "Adjusting memory size to: %s", humanize_bytes(size).c_str())
            m_torrent->memory_size(size);
        }

        // Set reserved pieces for first piece and last piece of file
        m_torrent->update_reserved_pieces({pieces.front(), pieces.back()});
    }

    OATPP_LOGI("File::start_buffer", "Buffering set: pieces=%d, size=%s", int(pieces.size()), humanize_bytes(size).c_str())

    std::scoped_lock guard(m_bufferMutex);
    m_buffer_pieces = std::move(pieces);
    m_buffer_size = size;
    m_buffer_progress = 0;
    m_isBuffering = true;
}

void File::stop_buffer() {
    std::scoped_lock guard(m_bufferMutex);
    m_isBuffering = false;
}

bool File::is_buffering() const {
    std::scoped_lock guard(m_bufferMutex);
    return m_isBuffering;
}

double File::buffer_progress() const {
    std::scoped_lock guard(m_bufferMutex);
    return m_buffer_progress;
}

std::int64_t File::buffer_size() const {
    std::scoped_lock guard(m_bufferMutex);
    return m_buffer_size;
}

void File::set_buffer_progress(double progress) {
    OATPP_LOGI("File::set_buffer_progress", "Updating buffer progress to %f", progress)

    std::scoped_lock guard(m_bufferMutex);
    m_buffer_progress = std::min(progress, 100.);
    if (m_buffer_progress >= 100) {
        OATPP_LOGI("File::set_buffer_progress", "Finished buffering")
        m_isBuffering = false;
    }
}

std::vector<int> File::buffer_pieces() const {
    std::scoped_lock guard(m_bufferMutex);
    return m_buffer_pieces;
}

} // namespace lh
