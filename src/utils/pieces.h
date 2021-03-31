#pragma once

#include <libtorrent/file_storage.hpp>
#include <libtorrent/units.hpp>

inline std::pair<lt::piece_index_t, lt::piece_index_t> region_pieces(std::int64_t offset, std::int64_t size, int piece_length,
                                                                     int pieces_count) {
    if (piece_length <= 0)
        return {0, 0};

    std::pair<lt::piece_index_t, lt::piece_index_t> result;

    result.first = std::max(0, int(offset / piece_length));
    result.second = std::min(pieces_count - 1, int((offset + size - 1) / piece_length));
    return result;
}

// lt::piece_index_t