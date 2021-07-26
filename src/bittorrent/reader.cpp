#include "reader.h"

#include <bittorrent/file.h>
#include <bittorrent/torrent.h>

#include <libtorrent/download_priority.hpp>
#include <utility>

namespace lh {

Reader::Reader(std::shared_ptr<Torrent> torrent, std::shared_ptr<File> file, oatpp::web::protocol::http::Range range)
    : m_torrent(std::move(torrent)), m_file(std::move(file)), m_range(std::move(range)) {
    m_id = std::stoi(get_random_numeric(7));
    OATPP_LOGI("Reader", "Creating reader for file: %s (%s)", m_file->path().c_str(), std::to_string(m_id).c_str());

    m_offset = m_file->offset();
    m_position = m_range.start;
    m_piece_size = m_torrent->piece_length();
    m_piece_start = piece_from_offset(m_range.start);
    m_piece_end = m_range.end > 0 ? piece_from_offset(m_range.end) : piece_from_offset(m_size - 1);
}

Reader::~Reader() {
    OATPP_LOGI("Reader", "Destroying: %s (%s)", m_file->path().c_str(), std::to_string(m_id).c_str());

    m_isClosing = true;

    // Unregister reader from parent objects
    m_file->unregister_reader(m_id);
    m_torrent->unregister_reader(m_id);
}

std::int64_t Reader::id() const {
    return m_id;
};

int Reader::piece_start() const {
    return m_piece_start;
};

int Reader::piece_end() const {
    return m_piece_end;
};

int Reader::piece_end_limit() const {
    return m_file->piece_end();
};

bool Reader::is_closing() const {
    return m_isClosing;
};

bool Reader::is_iterated() const {
    return m_isIterated;
};
    
void Reader::set_iterated(bool val) {
    m_isIterated = val;
};

oatpp::v_io_size Reader::read(void *buffer, v_buff_size bufferSize, oatpp::async::Action &action) {
    std::lock_guard<std::mutex> g(m_lock);

    (void)action;

    if (m_position >= m_file->size())
        return 0;

    m_piece_start = piece_from_offset(m_position);
    m_piece_end = piece_from_offset(m_position + bufferSize - 1);

    if (!m_isPrioritized) {
        m_isPrioritized = true;
        m_torrent->prioritize();
    }

    m_torrent->prioritize_pieces(m_piece_start, std::min(m_piece_start + 2, int(m_file->piece_end())));

    lt::storage_error ec;
    int ret = 0;
    int n = 0;
    int left = bufferSize;
    lt::iovec_t b = {(char *)buffer, left};

    for (int p = m_piece_start; p <= m_piece_end; ++p) {
        if (!m_torrent->have_piece(p)) {
            wait_for_piece(p);
            if (m_isClosing || m_torrent->is_closing() || !m_torrent->have_piece(p))
                return ret;
        }
        m_torrent->clear_piece_deadline(p);

        if (ret > 0)
            b = b.subspan(bufferSize-left);

        n = m_torrent->storage()->readv(b, p, piece_offset_from_offset(m_position), lt::open_mode::read_only, ec);
        if (ec) {
            OATPP_LOGI("Reader::read", "Error from readv: %s", ec.ec.message().c_str());
            return ret+n;
        }

        left -= n;
        m_position += n;
        ret += n;
    } 

    if (ret <= 0) {
        OATPP_LOGI("Reader::read", "Empty. Position=%s, n=%d, bs=%d", std::to_string(m_position).c_str(), ret, bufferSize);
    } else if (ret < bufferSize) {
        OATPP_LOGI("Reader::read", "Not enough. Offset=%s, Position=%s, Start=%d, End=%d, FStart=%d, FEnd=%d, n=%d, bs=%d", 
            std::to_string(m_offset).c_str(), std::to_string(m_position).c_str(), m_piece_start, m_piece_end, m_file->piece_start(), m_file->piece_end(), ret, bufferSize);
    }

    return ret;
}

int Reader::piece_from_offset(std::int64_t offset) const {
    if (m_piece_size <= 0)
        return 0;

    int p = int((m_offset + offset) / m_piece_size);
    if (p > m_file->piece_end())
        return m_file->piece_end();
    if (p < m_file->piece_start())
        return m_file->piece_start();
        
    return p;
}

std::int64_t Reader::piece_offset_from_offset(std::int64_t offset) const { return (m_offset + offset) % m_piece_size; }

std::int64_t Reader::piece_offset(int piece) const { return std::int64_t(piece) * m_piece_size - m_offset; }

void Reader::wait_for_piece(int piece) {
    OATPP_LOGI("Reader::wait_for_piece", "Waiting for piece: %d", piece);

    auto now = std::chrono::system_clock::now();
    while (!m_torrent->have_piece(piece)) {
        if (lh::is_closing || m_torrent->is_closing()) {
            OATPP_LOGI("Reader::wait_for_piece", "Abort waiting for piece '%d' due to closing", piece);
            return;
        }

        if (std::chrono::system_clock::now() - now > m_piece_timeout) {
            OATPP_LOGI("Reader::wait_for_piece", "Timed out waiting for piece '%d' with priority '%d'", piece,
                       m_torrent->piece_priority(piece));
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    OATPP_LOGI("Reader::wait_for_piece", "Done waiting for piece '%d' in %d ms", piece, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - now).count());
};

void Reader::set_piece_priority(int piece, int deadline, lt::download_priority_t priority) {
    if (m_torrent->piece_priority(piece) < priority) {
        m_torrent->piece_priority(piece, priority);
        m_torrent->set_piece_deadline(piece, deadline);
    }
}

void Reader::set_pieces_priorities(int piece, int pieces) {
    int end_piece = piece + pieces; //+ r.priorityPieces
    int i = 0;
    for (int p = piece; p <= end_piece && p <= m_piece_end; p++) {
        if (!m_torrent->have_piece(p)) {
            if (i <= pieces) {
                set_piece_priority(p, 0, lt::download_priority_t{7});
            } else {
                set_piece_priority(p, (i - pieces) * 10, lt::download_priority_t{6});
            }
        }

        i++;
    }
}

} // namespace lh