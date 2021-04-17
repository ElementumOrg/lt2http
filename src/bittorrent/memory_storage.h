#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include <oatpp/core/base/Environment.hpp>

#include <boost/dynamic_bitset.hpp>

#include <libtorrent/bencode.hpp>
#include <libtorrent/block_cache.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/file.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/piece_picker.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/torrent.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/units.hpp>
#include <utility>

#include <app/config.h>

#include <utils/numbers.h>
#include <utils/strings.h>

using Bitset = boost::dynamic_bitset<>;

namespace lh {

struct memory_piece {
  public:
    int index;
    int length;

    int size;
    int bi;
    bool is_completed;
    bool is_read;

    memory_piece(int i, int length);

    bool is_buffered() const;

    void reset();
};

struct memory_buffer {
  public:
    int index;
    int length;
    std::vector<char> buffer;

    int pi;
    bool is_used;
    std::chrono::time_point<std::chrono::system_clock> accessed;

    memory_buffer(int index, int length);

    bool is_assigned() const;

    void reset();
};

struct memory_storage : lt::storage_interface {
  private:
    std::mutex m_mutex;
    std::mutex r_mutex;

    Bitset reader_pieces;
    Bitset reserved_pieces;

    std::string id;
    std::int64_t capacity;

    int piece_count;
    std::int64_t piece_length;
    std::vector<memory_piece> pieces;

    int buffer_size;
    int buffer_limit;
    int buffer_used;
    int buffer_reserved;
    std::vector<memory_buffer> buffers;

    lt::file_storage m_files;
    std::shared_ptr<lt::torrent> m_handle;
    lt::torrent_handle m_torrent;

    bool is_logging;
    bool is_initialized;
    bool is_reading;
    bool is_handled;

  public:
    explicit memory_storage(lt::storage_params const &params);

    void initialize(lt::storage_error &ec) override;

    std::int64_t get_memory_size() const;

    void set_memory_size(std::int64_t s);

    int get_buffers_count() const;

    int readv(lt::span<lt::iovec_t const> bufs, lt::piece_index_t pi, int offset, lt::open_mode_t  /*mode*/,
              lt::storage_error &ec) override;

    int writev(lt::span<lt::iovec_t const> bufs, lt::piece_index_t pi, int offset, lt::open_mode_t  /*mode*/,
               lt::storage_error &ec) override;

    void rename_file(lt::file_index_t index, std::string const &new_filename, lt::storage_error &ec) override;

    lt::status_t move_storage(std::string const & /*save_path*/, lt::move_flags_t /*flags*/, lt::storage_error & /*ec*/) override;

    bool verify_resume_data(lt::add_torrent_params const & /*rd*/, lt::aux::vector<std::string, lt::file_index_t> const & /*links*/,
                            lt::storage_error & /*ec*/) override;

    bool write_resume_data(lt::entry & /*rd*/) const;

    void write_resume_data(lt::entry &rd, lt::storage_error &ec);

    void release_files(lt::storage_error &ec) override;

    void delete_files(lt::remove_flags_t /*options*/, lt::storage_error & /*ec*/) override;

    bool has_any_file(lt::storage_error & /*ec*/) override;

    void set_torrent_handle(const lt::torrent_handle& th);

    void set_file_priority(lt::aux::vector<lt::download_priority_t, lt::file_index_t> & /*prio*/, lt::storage_error & /*ec*/) override;

    int move_storage(std::string const & /*save_path*/, int  /*flags*/, lt::storage_error &ec);

    void write_resume_data(lt::entry &rd, lt::storage_error &ec) const;

    void delete_files(int options, lt::storage_error &ec);

    bool get_read_buffer(memory_piece *p);

    bool get_write_buffer(memory_piece *p);

    bool get_buffer(memory_piece *p, bool is_write);

    void trim(int pi);

    std::string get_buffer_info();

    int find_last_buffer(int pi, bool check_read);

    void remove_piece(int bi);

    void restore_piece(int pi);

    void enable_logging();

    void disable_logging();

    void update_reader_pieces(const std::vector<int>& pieces);

    void update_reserved_pieces(const std::vector<int>& pieces);

    bool is_reserved(int index) const;

    bool is_readered(int index);
};

lt::storage_interface *memory_storage_constructor(lt::storage_params const &params, lt::file_pool &);

} // namespace lh
