#include "file.h"

#include <libtorrent/download_priority.hpp>

#include <oatpp/core/Types.hpp>

#include <memory>
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
      m_size(file_size), m_offset(file_offset), m_isSelected(priority > 0), m_isBuffering(false) {
    m_config = lh::config();

    m_start_buffer_size = m_config.buffer_size * 1024 * 1024;
    m_end_buffer_size = m_config.end_buffer_size * 1024 * 1024;

    auto region = region_pieces(m_offset, m_size, m_torrent->piece_length(), m_torrent->pieces_count());

    m_piece_start = region.first;
    m_piece_end = region.second;
}

File::~File() = default;

lt::file_index_t File::index() const { return m_index; }

std::string File::path() const { return m_path; }

std::string File::name() const { return m_name; }

std::int64_t File::size() const { return m_size; }

std::int64_t File::offset() const { return m_offset; }

std::string File::stream_uri() const {
    return Fmt("/torrents/%s/files/%d/stream/%s", m_torrent->hash().c_str(), m_index, uri_escape(std::string(m_name)).c_str());
}

lt::piece_index_t File::piece_start() const { return m_piece_start; }

lt::piece_index_t File::piece_end() const { return m_piece_end; }

lt::download_priority_t File::priority() const { return m_torrent->file_priority(index()); }

void File::set_priority(lt::download_priority_t priority) {
    m_isSelected = priority > 0;

    if (!m_torrent->is_memory_storage())
        m_torrent->file_priority(m_index, priority);
}

void File::add_buffer_piece(int piece) {
    if (std::find(m_buffer_pieces.begin(), m_buffer_pieces.end(), piece) != m_buffer_pieces.end())
        return;

    m_torrent->piece_priority(piece, lt::top_priority);
    m_torrent->set_piece_deadline(piece, 0);
    m_buffer_size += m_torrent->piece_length_at(piece);
    m_buffer_pieces.push_back(piece);
}

void File::start_buffer() {
    OATPP_LOGI("File::start_buffer", "Buffering file: %s/%s", m_torrent->hash().c_str(), m_name.c_str())

    m_isBuffering = true;
    m_buffer_progress = 0;
    m_buffer_pieces.clear();
    m_buffer_size = 0;

    if (m_size >= m_start_buffer_size + m_end_buffer_size) {
        auto range_start = region_pieces(m_offset, m_start_buffer_size, m_torrent->piece_length(), m_torrent->pieces_count());
        for (int i = range_start.first; i <= range_start.second; i++) {
            add_buffer_piece(i);
        }

        auto range_end = region_pieces(m_offset + m_size - m_end_buffer_size, m_end_buffer_size, m_torrent->piece_length(),
                                       m_torrent->pieces_count());
        for (int i = range_end.first; i <= range_end.second; i++) {
            add_buffer_piece(i);
        }
    } else {
        auto range = region_pieces(m_offset, m_size, m_torrent->piece_length(), m_torrent->pieces_count());
        for (int i = range.first; i <= range.second; i++) {
            add_buffer_piece(i);
        }
    }

    if (m_torrent->is_memory_storage()) {
        // TODO: Implement adjusting of memory to fit more pieces
        if (m_config.auto_adjust_memory_size) {
        }

        if (m_buffer_size > m_torrent->memory_size()) {
            OATPP_LOGI("File::start_buffer", "Adjusting memory size to: %s", humanize_bytes(m_buffer_size).c_str())
            m_torrent->memory_size(m_buffer_size);
        }

        // Set reserved pieces for first piece and last piece of file
        std::vector<int> reserved{m_buffer_pieces.front(), m_buffer_pieces.back()};
        ((lh::memory_storage*) m_torrent->storage())->update_reserved_pieces(reserved);
    }

    OATPP_LOGI("File::start_buffer", "Buffering set: pieces=%d, size=%s", m_buffer_pieces.size(),
               humanize_bytes(m_buffer_size).c_str())
}

void File::stop_buffer() { m_isBuffering = false; }

bool File::is_buffering() const { return m_isBuffering; };

double File::buffer_progress() const { return m_buffer_progress; };

std::int64_t File::buffer_size() const { return m_buffer_size; };

void File::set_buffer_progress(double progress) { 
    OATPP_LOGI("File::set_buffer_progress", "Updating buffer progress to %f", progress)
    m_buffer_progress = progress; 
    if (m_buffer_progress >= 100) {
        OATPP_LOGI("File::set_buffer_progress", "Finished buffering")
        m_buffer_progress = 100;
        m_isBuffering = false;
    }
};

std::vector<int> File::buffer_pieces() const { return m_buffer_pieces; };

bool File::has_readers() const { return !m_readers.empty(); }

void File::register_reader(std::int64_t id, Reader* reader) { 
    m_readers[id] = reader; 
}

void File::unregister_reader(const std::int64_t id) {
    if (m_readers.find(id) != m_readers.end()) {
        m_readers.erase(id);
    }
}

} // namespace lh