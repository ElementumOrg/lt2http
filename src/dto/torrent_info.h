#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include "torrent_status.h"

#include "bittorrent/torrent.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class TorrentInfoDto : public oatpp::DTO {

    DTO_INIT(TorrentInfoDto, DTO)

    DTO_FIELD_INFO(hash) {
        info->description = "Torrent hash";
    }
    DTO_FIELD(String, hash);

    DTO_FIELD_INFO(name) {
        info->description = "Name of a torrent file";
    }
    DTO_FIELD(String, name);

    DTO_FIELD_INFO(size) {
        info->description = "Total torrent size (bytes)";
    }
    DTO_FIELD(Int64, size);

    DTO_FIELD_INFO(has_metadata) {
        info->description = "Does this torrent have metadata (bool)";
    }
    DTO_FIELD(Boolean, has_metadata);

    DTO_FIELD(Object<TorrentStatusDto>, torrent_status);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::Object<TorrentInfoDto> torrentInfoDtoFromTorrent(const std::shared_ptr<lh::Torrent>& torrent, bool is_status) {
    auto dto = TorrentInfoDto::createShared();

    dto->hash = torrent->hash().c_str();
    dto->name = torrent->name().c_str();
    dto->size = torrent->size();
    dto->has_metadata = torrent->has_metadata();

    if (is_status)
        dto->torrent_status = torrentStatusDtoFromTorrent(torrent);

    return dto;
}