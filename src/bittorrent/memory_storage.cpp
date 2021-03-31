#include "memory_storage.h"

namespace lh {

memory_piece::memory_piece(int i, int length) : index(i), length(length) {
    size = 0;
    bi = -1;
    is_completed = false;
    is_read = false;
};

bool memory_piece::is_buffered() const { return bi != -1; };

void memory_piece::reset() {
    bi = -1;
    is_completed = false;
    is_read = false;
    size = 0;
}

memory_buffer::memory_buffer(int index, int length) : index(index), length(length) {
    pi = -1;
    is_used = false;

    buffer.resize(length);
};

bool memory_buffer::is_assigned() const { return pi != -1; };

void memory_buffer::reset() {
    is_used = false;
    pi = -1;
    accessed = std::chrono::system_clock::now();
    std::fill(buffer.begin(), buffer.end(), '\0');
};

memory_storage::memory_storage(lt::storage_params const &params) : lt::storage_interface(params.files) {
    piece_count = 0;
    piece_length = 0;

    buffer_size = 0;
    buffer_limit = 0;
    buffer_used = 0;
    buffer_reserved = 0;

    is_logging = false;
    is_initialized = false;
    is_reading = false;

    m_files = params.files;

    capacity = lh::memory_size;
    piece_count = m_files.num_pieces();
    piece_length = m_files.piece_length();

    OATPP_LOGI("memory_storage", "Creating memory storage with size: %s, Pieces count: %d, Piece size: %s",
                humanize_bytes(capacity).c_str(), piece_count, humanize_bytes(piece_length).c_str());

    for (int i = 0; i < piece_count; i++) {
        pieces.emplace_back(i, m_files.piece_size(lt::piece_index_t(i)));
    }

    // Using max possible buffers + 2
    buffer_size = int(rint(ceil(capacity / double(piece_length))));
    if (buffer_size > piece_count) {
        buffer_size = piece_count;
    };
    buffer_limit = buffer_size;
    OATPP_LOGI("memory_storage", "Using %s memory for %d buffer items", humanize_bytes(buffer_size * piece_length).c_str(),
                buffer_size);

    reader_pieces.resize(piece_count + 10);
    reserved_pieces.resize(piece_count + 10);
};

void memory_storage::initialize(lt::storage_error & /*ec*/) {
    for (int i = 0; i < buffer_size; i++) {
        buffers.emplace_back(i, piece_length);
    }

    reader_pieces.resize(piece_count + 10);
    reserved_pieces.resize(piece_count + 10);

    is_initialized = true;
}

std::int64_t memory_storage::get_memory_size() const { return capacity; }

void memory_storage::set_memory_size(std::int64_t s) {
    if (s <= capacity)
        return;

    std::lock_guard<std::mutex> guard(m_mutex);

    capacity = s;

    int prev_buffer_size = buffer_size;

    // Using max possible buffers + 2
    buffer_size = int(rint(ceil(capacity / double(piece_length))));
    if (buffer_size > piece_count) {
        buffer_size = piece_count;
    };
    buffer_limit = buffer_size;
    if (prev_buffer_size == buffer_size) {
        OATPP_LOGI("memory_storage::set_memory_size", "Not increasing buffer due to same size (%s)",
                    humanize_bytes(buffer_size).c_str());
        return;
    };

    OATPP_LOGI("memory_storage::set_memory_size", "Increasing buffer to %s buffers", humanize_bytes(buffer_size).c_str());

    for (int i = prev_buffer_size; i < buffer_size; i++) {
        buffers.emplace_back(i, piece_length);
    }
}

int memory_storage::get_buffers_count() const {
    return buffer_size;
};

int memory_storage::readv(lt::span<lt::iovec_t const> bufs, lt::piece_index_t const pi, int offset, lt::open_mode_t  /*mode*/,
            lt::storage_error & /*ec*/) {
    if (!is_initialized)
        return 0;

    auto piece = static_cast<LTUnderlyingType<lt::piece_index_t>>(pi);
    if (is_logging) {
        OATPP_LOGI("memory_storage::readv", "readv in  p: %d, off: %d", piece, offset)
    };

    if (!get_read_buffer(&pieces[piece])) {
        OATPP_LOGE("memory_storage::readv", "Not buffered piece: %d", piece)
        return 0;
    };

    int size = 0;
    int file_offset = offset;
    int n = 0;
    for (lt::iovec_t const &b : bufs) {
        size += b.size();

        int const to_copy = std::min(buffers[pieces[piece].bi].buffer.size() - file_offset, std::size_t(b.size()));
        memcpy(b.data(), &buffers[pieces[piece].bi].buffer[file_offset], to_copy);
        file_offset += to_copy;
        n += to_copy;
    };

    if (is_logging) {
        OATPP_LOGI("memory_storage::readv", "readv out p: %d, pl: %d, bufs: %d/%d, off: %d, bs: %d, res: %d=%d", piece,
                    pieces[piece].length, bufs.size(), bufs[0].size(), offset, buffers[pieces[piece].bi].buffer.size(), size,
                    n);
    };

    if (pieces[piece].is_completed && offset + n >= pieces[piece].size) {
        pieces[piece].is_read = true;
    };

    buffers[pieces[piece].bi].accessed = std::chrono::system_clock::now();

    return n;
};

int memory_storage::writev(lt::span<lt::iovec_t const> bufs, lt::piece_index_t const pi, int offset, lt::open_mode_t  /*mode*/,
            lt::storage_error & /*ec*/) {
    auto piece = static_cast<LTUnderlyingType<lt::piece_index_t>>(pi);
    if (is_logging) {
        OATPP_LOGI("memory_storage::writev", "writev in  p: %d, off: %d, bufs: %d", piece, offset, bufs.size());
    };

    if (!is_initialized)
        return 0;

    if (!get_write_buffer(&pieces[piece])) {
        if (is_logging) {
            OATPP_LOGI("memory_storage::writev", "no write buffer: %d", piece);
        };
        return 0;
    };

    int size = 0;

    int file_offset = offset;
    int n = 0;
    for (const auto &b : bufs) {
        size += b.size();

        int const to_copy = std::min(std::size_t(pieces[piece].length) - file_offset, std::size_t(b.size()));
        std::memcpy(&buffers[pieces[piece].bi].buffer[file_offset], b.data(), to_copy);

        file_offset += to_copy;
        n += to_copy;
    };

    if (is_logging) {
        OATPP_LOGI("memory_storage::writev", "writev out p: %d, pl: %d, bufs: %d / %d, req: %d, off: %d, bs: %d, res: %d=%d",
                    piece, pieces[piece].length, bufs.size(), bufs[0].size(), size, offset,
                    buffers[pieces[piece].bi].buffer.size(), size, n);
    };

    pieces[piece].size += n;
    buffers[pieces[piece].bi].accessed = std::chrono::system_clock::now();

    if (buffer_used >= buffer_limit) {
        trim(piece);
    }

    return n;
};

void memory_storage::rename_file(lt::file_index_t index, std::string const &new_filename, lt::storage_error &ec) {}

lt::status_t memory_storage::move_storage(std::string const & /*save_path*/, lt::move_flags_t /*flags*/, lt::storage_error & /*ec*/) {
    return lt::status_t::no_error;
}

bool memory_storage::verify_resume_data(lt::add_torrent_params const & /*rd*/, lt::aux::vector<std::string, lt::file_index_t> const & /*links*/,
                        lt::storage_error & /*ec*/) {
    return false;
}

bool memory_storage::write_resume_data(lt::entry & /*rd*/) const { return false; }

void memory_storage::write_resume_data(lt::entry &rd, lt::storage_error &ec) {}

void memory_storage::release_files(lt::storage_error &ec) {}

void memory_storage::delete_files(lt::remove_flags_t /*options*/, lt::storage_error & /*ec*/) {}

bool memory_storage::has_any_file(lt::storage_error & /*ec*/) { return false; }

void memory_storage::set_torrent_handle(const lt::torrent_handle& th) {
    m_torrent = th;
    m_handle = th.native_handle();

    is_handled = true;
}

void memory_storage::set_file_priority(lt::aux::vector<lt::download_priority_t, lt::file_index_t> & /*prio*/, lt::storage_error & /*ec*/) {}

int memory_storage::move_storage(std::string const & /*save_path*/, int  /*flags*/, lt::storage_error & /*ec*/) { return 0; }

void memory_storage::write_resume_data(lt::entry &rd, lt::storage_error &ec) const {}

void memory_storage::delete_files(int options, lt::storage_error &ec){};

bool memory_storage::get_read_buffer(memory_piece *p) { return get_buffer(p, false); };

bool memory_storage::get_write_buffer(memory_piece *p) { return get_buffer(p, true); };

bool memory_storage::get_buffer(memory_piece *p, bool is_write) {
    if (p->is_buffered())
        return true;

    if (!is_write) {
        // Trying to lock and get to make sure we are not affected
        // by write/read at the same time.
        std::lock_guard<std::mutex> guard(m_mutex);
        return p->is_buffered();
    }

    std::lock_guard<std::mutex> guard(m_mutex);

    // Once again checking in case we had multiple writes in parallel
    if (p->is_buffered())
        return true;

    // Check if piece is not in reader ranges and avoid allocation
    if (is_reading && !is_readered(p->index)) {
        restore_piece(p->index);
        return false;
    }

    for (int i = 0; i < buffer_size; i++) {
        if (buffers[i].is_used) {
            continue;
        };

        if (is_logging) {
            OATPP_LOGI("memory_storage::get_buffer", "Setting buffer %d to piece %d", buffers[i].index, p->index);
        };

        buffers[i].is_used = true;
        buffers[i].pi = p->index;
        buffers[i].accessed = std::chrono::system_clock::now();

        p->bi = buffers[i].index;

        // If we are placing permanent buffer entry - we should reduce the
        // limit, to properly check for the usage.
        if (reserved_pieces.test(p->index)) {
            buffer_limit--;
        } else {
            buffer_used++;
        };

        break;
    }

    return p->is_buffered();
};

void memory_storage::trim(int pi) {
    if (capacity < 0 || buffer_used < buffer_limit) {
        return;
    };

    std::lock_guard<std::mutex> guard(m_mutex);

    while (buffer_used >= buffer_limit) {
        if (is_logging) {
            OATPP_LOGI("memory_storage::trim", "Trimming %d to %d with reserved %d, %s", buffer_used, buffer_limit,
                        buffer_reserved, get_buffer_info().c_str());
        };

        if (!reader_pieces.empty()) {
            int bi = find_last_buffer(pi, true);
            if (bi != -1) {
                if (is_logging) {
                    OATPP_LOGI("memory_storage::trim", "Removing non-read piece: %d, buffer:", buffers[bi].pi, bi);
                };
                remove_piece(bi);
                continue;
            }
        }

        int bi = find_last_buffer(pi, false);
        if (bi != -1) {
            if (is_logging) {
                OATPP_LOGI("memory_storage::trim", "Removing LRU piece: %d, buffer: %d", buffers[bi].pi, bi);
            };
            remove_piece(bi);
            continue;
        }
    }
};

std::string memory_storage::get_buffer_info() {
    std::string result;

    for (auto &buffer : buffers) {
        if (!result.empty())
            result += " ";

        result += std::to_string(buffer.index) + ":" + std::to_string(buffer.pi);
    };

    return result;
};

int memory_storage::find_last_buffer(int pi, bool check_read) {
    int bi = -1;
    std::chrono::time_point<std::chrono::system_clock> minTime = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> guard(r_mutex);

    for (auto &buffer : buffers) {
        if (buffer.is_used && buffer.is_assigned() && !is_reserved(buffer.pi) && buffer.pi != pi &&
            (!check_read || !is_readered(buffer.pi)) && buffer.accessed < minTime) {
            bi = buffer.index;
            minTime = buffer.accessed;
        };
    };

    return bi;
}

void memory_storage::remove_piece(int bi) {
    int pi = buffers[bi].pi;

    buffers[bi].reset();
    buffer_used--;

    if (pi != -1 && pi < piece_count) {
        pieces[pi].reset();
        restore_piece(pi);
    }
}

void memory_storage::restore_piece(int pi) {
    if (!is_handled)
        return;

    // TODO: Cleanup everything here when it starts to work fine without segfaults.
    // if (m_handle == nullptr || m_torrent == nullptr)
    //     return;

    if (is_logging) {
        OATPP_LOGI("memory_storage::restore_piece", "Restoring piece: %d", pi);
    };
    // t->picker().reset_piece(pi);
    auto pit = lt::piece_index_t(pi);
    m_torrent.reset_piece_deadline(pit);
    // m_torrent.piece_priority(pit, lt::download_priority_t{0});
    // m_handle->reset_piece_deadline(pit);
    // m_handle->set_piece_priority(pit, lt::download_priority_t{0});
    // m_torrent.
    // m_handle->piece_priority(pit, lt::download_priority_t{0});
    // m_handle->set_piece_priority(pit, lt::download_priority_t{0});
    m_handle->picker().set_piece_priority(pit, lt::download_priority_t(0));
    m_handle->picker().we_dont_have(pit);
    // if (m_handle->picker().have_piece(pit))
    // m_torrent.picker().we_dont_have(pit);
    // m_handle->picker().we_dont_have(pit);
    // m_handle->picker().restore_piece(pit);
}

void memory_storage::enable_logging() { is_logging = true; }

void memory_storage::disable_logging() { is_logging = false; }

void memory_storage::update_reader_pieces(const std::vector<int>& pieces) {
    if (!is_initialized)
        return;

    std::lock_guard<std::mutex> guard(r_mutex);
    reader_pieces.reset();
    for (int piece : pieces) {
        reader_pieces.set(piece);
    };
};

void memory_storage::update_reserved_pieces(const std::vector<int>& pieces) {
    if (!is_initialized)
        return;

    std::lock_guard<std::mutex> guard(r_mutex);
    buffer_reserved = 0;
    reserved_pieces.reset();
    for (int piece : pieces) {
        reserved_pieces.set(piece);
        buffer_reserved++;
    };
};

bool memory_storage::is_reserved(int index) const {
    if (!is_initialized)
        return false;

    return reserved_pieces.test(index);
};

bool memory_storage::is_readered(int index) {
    if (!is_initialized)
        return false;

    if (m_handle == nullptr) {
        OATPP_LOGI("memory_storage::is_readered", "no torrent handle");
        return true;
    }

    return m_handle->piece_priority(lt::piece_index_t(index)) != lt::download_priority_t(0);
};

lt::storage_interface* memory_storage_constructor(lt::storage_params const &params, lt::file_pool & /*unused*/) {
    return new memory_storage(params);
}

}