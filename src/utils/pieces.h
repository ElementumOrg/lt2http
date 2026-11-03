#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>

#include <libtorrent/units.hpp>

// First and last piece of a byte region.
inline std::pair<lt::piece_index_t, lt::piece_index_t> region_pieces(std::int64_t offset, std::int64_t size, int piece_length,
                                                                     int pieces_count) {
    if (piece_length <= 0)
        return {lt::piece_index_t{0}, lt::piece_index_t{0}};

    return {lt::piece_index_t{std::max(0, int(offset / piece_length))},
            lt::piece_index_t{std::min(pieces_count - 1, int((offset + size - 1) / piece_length))}};
}
