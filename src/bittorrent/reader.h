#pragma once

#include <chrono>
#include <mutex>

#include <libtorrent/download_priority.hpp>
#include <libtorrent/error_code.hpp>

#include <oatpp/web/protocol/http/outgoing/StreamingBody.hpp>
#include <oatpp/core/Types.hpp>
#include <oatpp/core/base/Environment.hpp>

#include <app/config.h>

namespace lh {

class File;
class Torrent;

class Reader : public oatpp::data::stream::ReadCallback {
  private:
    std::shared_ptr<Torrent> m_torrent;
    std::shared_ptr<File> m_file;
    oatpp::web::protocol::http::Range m_range;

    std::int64_t m_id;

    std::mutex m_lock;
    std::int64_t m_offset = 0;
    std::int64_t m_size = 0;
    std::int64_t m_piece_size = 0;
    std::int64_t m_position = 0;
    int m_piece_start = 0;
    int m_piece_end = 0;

    bool m_isClosing = false;
    bool m_isPrioritized = false;

    std::chrono::seconds m_piece_timeout{60};

  public:
    Reader(std::shared_ptr<Torrent> torrent, std::shared_ptr<File> file, oatpp::web::protocol::http::Range range);
    ~Reader() override;

    std::int64_t id() const;
    int piece_start() const;
    int piece_end() const;
    int piece_end_limit() const;

    bool is_closing() const;

    oatpp::v_io_size read(void *buffer, v_buff_size bufferSize, oatpp::async::Action &action) override;

    int piece_from_offset(std::int64_t offset) const;
    std::int64_t piece_offset_from_offset(std::int64_t offset) const;
    std::int64_t piece_offset(int piece) const;
    void wait_for_piece(int piece);
    void set_piece_priority(int piece, int deadline, lt::download_priority_t priority);
    void set_pieces_priorities(int piece, int pieces);
};

} // namespace lh