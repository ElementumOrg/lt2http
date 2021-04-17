#pragma once

#include <map>
#include <memory>

#include <libtorrent/download_priority.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/units.hpp>

#include <app/config.h>

namespace lh {

class Reader;

class Torrent;

class File {
  private:
    Torrent* m_torrent;

    bool m_isSelected;

    lt::file_index_t m_index = 0;
    std::string m_path;
    std::string m_name;
    std::int64_t m_size = 0;
    std::int64_t m_offset = 0;

    lt::piece_index_t m_piece_start = 0;
    lt::piece_index_t m_piece_end = 0;

    bool m_isBuffering;
    std::vector<int> m_buffer_pieces;
    std::int64_t m_buffer_size = 0;
    double m_buffer_progress = 0;
    std::int64_t m_start_buffer_size = 0;
    std::int64_t m_end_buffer_size = 0;

    Config m_config;
    std::map<std::int64_t, Reader*> m_readers;

  public:
    File(Torrent* torrent, lt::file_index_t index, std::string file_path, std::string file_name, std::int64_t file_size,
         std::int64_t file_offset, lt::download_priority_t priority);
    ~File();

    lt::file_index_t index() const;
    std::string path() const;
    std::string name() const;
    std::int64_t size() const;
    std::int64_t offset() const;
    std::string stream_uri() const;

    lt::piece_index_t piece_start() const;
    lt::piece_index_t piece_end() const;

    lt::download_priority_t priority() const;
    void set_priority(lt::download_priority_t priority);
    void add_buffer_piece(int piece);

    void start_buffer();
    void stop_buffer();

    bool is_buffering() const;
    double buffer_progress() const;
    std::int64_t buffer_size() const;
    void set_buffer_progress(double progress);
    std::vector<int> buffer_pieces() const;

    bool has_readers() const;
    void register_reader(std::int64_t id, Reader* reader);
    void unregister_reader(std::int64_t id);
};

} // namespace lh