#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "torrent_status.h"

#include "bittorrent/torrent.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class TorrentInfoDto : public oatpp::DTO {

    DTO_INIT(TorrentInfoDto, DTO)

    DTO_FIELD_INFO(hash) { info->description = "Torrent hash"; }
    DTO_FIELD(String, hash);

    DTO_FIELD_INFO(name) { info->description = "Name of a torrent file"; }
    DTO_FIELD(String, name);

    DTO_FIELD_INFO(size) { info->description = "Total torrent size (bytes)"; }
    DTO_FIELD(Int64, size);

    DTO_FIELD_INFO(download_path) { info->description = "Folder of the files on disk, null for memory storage"; }
    DTO_FIELD(String, download_path);

    DTO_FIELD_INFO(piece_length) { info->description = "Piece size (bytes), -1 without metadata"; }
    DTO_FIELD(Int32, piece_length);

    DTO_FIELD_INFO(has_metadata) { info->description = "Does this torrent have metadata (bool)"; }
    DTO_FIELD(Boolean, has_metadata);

    DTO_FIELD_INFO(is_memory_storage) { info->description = "Does this torrent run with memory storage (bool)"; }
    DTO_FIELD(Boolean, is_memory_storage);

    DTO_FIELD_INFO(memory_size) { info->description = "Memory used by the memory storage (bytes)"; }
    DTO_FIELD(Int64, memory_size);

    DTO_FIELD_INFO(is_paused) { info->description = "Is this torrent paused (bool)"; }
    DTO_FIELD(Boolean, is_paused);

    DTO_FIELD_INFO(total_files) { info->description = "Number of files"; }
    DTO_FIELD(Int32, total_files);

    DTO_FIELD_INFO(chosen_files) { info->description = "Number of files selected for download"; }
    DTO_FIELD(Int32, chosen_files);

    DTO_FIELD_INFO(players) { info->description = "Players registered with /torrents/{infoHash}/players/add"; }
    DTO_FIELD(Int32, players);

    DTO_FIELD_INFO(readers) { info->description = "Active stream connections"; }
    DTO_FIELD(Int32, readers);

    DTO_FIELD_INFO(meta_info) { info->description = "JSON set with /torrents/{infoHash}/meta_info, null if not set"; }
    DTO_FIELD(Any, meta_info);

    DTO_FIELD(Object<TorrentStatusDto>, torrent_status);
};

#include OATPP_CODEGEN_END(DTO)

// The JSON is checked when it is set, so only a broken .meta file gives null
inline oatpp::Any metaInfoFromJson(const std::string &json) {
    if (json.empty())
        return nullptr;

    static const auto mapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    try {
        return mapper->readFromString<oatpp::Any>(json);
    } catch (const std::exception &e) {
        OATPP_LOGE("metaInfoFromJson", "Invalid meta info: %s", e.what())
        return nullptr;
    }
}

// lang: a language like en_gb, for the localized state
inline oatpp::Object<TorrentInfoDto> torrentInfoDtoFromTorrent(const std::shared_ptr<lh::Torrent> &torrent, bool is_status,
                                                               const std::string &lang) {
    auto dto = TorrentInfoDto::createShared();

    dto->hash = torrent->hash();
    dto->name = torrent->name();
    dto->size = torrent->size();
    if (auto path = torrent->download_path(); !path.empty())
        dto->download_path = path;
    dto->piece_length = torrent->piece_length();
    dto->has_metadata = torrent->has_metadata();
    dto->is_memory_storage = torrent->is_memory_storage();
    if (torrent->is_memory_storage())
        dto->memory_size = torrent->memory_size();
    dto->is_paused = torrent->is_paused();
    dto->total_files = int(torrent->files().size());
    dto->chosen_files = torrent->files_chosen();
    dto->players = torrent->players();
    dto->readers = torrent->readers_count();
    dto->meta_info = metaInfoFromJson(torrent->meta_info());

    if (is_status)
        dto->torrent_status = torrentStatusDtoFromTorrent(torrent, lang);

    return dto;
}