#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include "bittorrent/torrent.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class TorrentStatusDto : public oatpp::DTO {

    DTO_INIT(TorrentStatusDto, DTO)

    DTO_FIELD_INFO(total) {
        info->description = "Total bytes in a torrent";
    }
    DTO_FIELD(Int64, total);

    DTO_FIELD_INFO(total_done) {
        info->description = "Total bytes, downloaded";
    }
    DTO_FIELD(Int64, total_done);

    DTO_FIELD_INFO(total_wanted) {
        info->description = "Total bytes, selected for download";
    }
    DTO_FIELD(Int64, total_wanted);

    DTO_FIELD_INFO(total_wanted_done) {
        info->description = "Total bytes, selected for download, finished";
    }
    DTO_FIELD(Int64, total_wanted_done);

    DTO_FIELD_INFO(progress) {
        info->description = "Download progress (0 to 100)";
    }
    DTO_FIELD(Float64, progress);

    DTO_FIELD_INFO(download_rate) {
        info->description = "Current upload rate (bytes)";
    }
    DTO_FIELD(Int64, download_rate);

    DTO_FIELD_INFO(upload_rate) {
        info->description = "Current upload rate (bytes)";
    }
    DTO_FIELD(Int64, upload_rate);

    DTO_FIELD_INFO(state) {
        info->description = "Torrent State (as int)";
    }
    DTO_FIELD(Int32, state);

    DTO_FIELD_INFO(state_string) {
        info->description = "Torrent State (as a string)";
    }
    DTO_FIELD(String, state_string);

    DTO_FIELD_INFO(seeders) {
        info->description = "Connected seeders";
    }
    DTO_FIELD(Int32, seeders);

    DTO_FIELD_INFO(seeders_total) {
        info->description = "Total number of seeders";
    }
    DTO_FIELD(Int32, seeders_total);

    DTO_FIELD_INFO(peers) {
        info->description = "Connected peers";
    }
    DTO_FIELD(Int32, peers);

    DTO_FIELD_INFO(peers_total) {
        info->description = "Total number of peers";
    }
    DTO_FIELD(Int32, peers_total);
    
    DTO_FIELD_INFO(seeding_time) {
        info->description = "Torrent is being seeding time (seconds)";
    }
    DTO_FIELD(Int32, seeding_time);

    DTO_FIELD_INFO(finished_time) {
        info->description = "Torrent is being finished time (seconds)";
    }
    DTO_FIELD(Int32, finished_time);

    DTO_FIELD_INFO(active_time) {
        info->description = "Torrent is being active time (seconds)";
    }
    DTO_FIELD(Int32, active_time);

    DTO_FIELD_INFO(total_download) {
        info->description = "Total downloaded (bytes)";
    }
    DTO_FIELD(Int64, total_download);

    DTO_FIELD_INFO(total_upload) {
        info->description = "Total uploaded (bytes)";
    }
    DTO_FIELD(Int64, total_upload);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::Object<TorrentStatusDto> torrentStatusDtoFromTorrent(const std::shared_ptr<lh::Torrent>& torrent) {
    auto dto = TorrentStatusDto::createShared();

    dto->total = torrent->total();
    dto->total_done = torrent->total_done();
    dto->total_wanted = torrent->total_wanted();
    dto->total_wanted_done = torrent->total_wanted_done();

    dto->progress = torrent->progress();

    dto->download_rate = torrent->download_rate();
    dto->upload_rate = torrent->upload_rate();

    dto->state = int(torrent->state());
    dto->state_string = torrent->state_string().c_str();

    dto->seeders = torrent->seeds_count();
    dto->seeders_total = torrent->total_seeds_count();
    dto->peers = torrent->peers_count();
    dto->peers_total = torrent->total_peers_count();

    dto->seeding_time = torrent->seeding_time();
    dto->finished_time = torrent->finished_time();
    dto->active_time = torrent->active_time();

    dto->total_download = torrent->total_download();
    dto->total_upload = torrent->total_upload();

    return dto;
}