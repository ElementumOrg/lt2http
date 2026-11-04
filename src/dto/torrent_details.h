#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "bittorrent/torrent.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class TrackerStatusDto : public oatpp::DTO {

    DTO_INIT(TrackerStatusDto, DTO)

    DTO_FIELD_INFO(url) { info->description = "Tracker url, DHT for the DHT"; }
    DTO_FIELD(String, url);

    DTO_FIELD_INFO(tier) { info->description = "Tier, -1 for DHT"; }
    DTO_FIELD(Int32, tier);

    DTO_FIELD_INFO(status) { info->description = "working, updating, failed, not_contacted or off"; }
    DTO_FIELD(String, status);

    DTO_FIELD_INFO(fails) { info->description = "Failed announces in a row"; }
    DTO_FIELD(Int32, fails);

    DTO_FIELD_INFO(seeds) { info->description = "Seeds from the scrape, -1 if unknown"; }
    DTO_FIELD(Int32, seeds);

    DTO_FIELD_INFO(peers) { info->description = "Peers from the scrape, -1 if unknown"; }
    DTO_FIELD(Int32, peers);

    DTO_FIELD_INFO(downloaded) { info->description = "Completed downloads from the scrape, -1 if unknown"; }
    DTO_FIELD(Int32, downloaded);

    DTO_FIELD_INFO(next_announce) { info->description = "Seconds to the next announce, -1 if unknown"; }
    DTO_FIELD(Int32, next_announce);

    DTO_FIELD_INFO(message) { info->description = "Last message or error"; }
    DTO_FIELD(String, message);
};

class PiecesStatusDto : public oatpp::DTO {

    DTO_INIT(PiecesStatusDto, DTO)

    DTO_FIELD_INFO(map) {
        info->description = "A character per piece: * stored and read by a stream, ? wanted by a stream, + stored, ~ downloading, "
                            "1-7 queued with that priority, - not wanted";
    }
    DTO_FIELD(String, map);

    DTO_FIELD(Int32, total);
    DTO_FIELD(Int32, stored);
    DTO_FIELD(Int32, downloading);
    DTO_FIELD(Int32, prioritized);
    DTO_FIELD(Int32, readers);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::List<oatpp::Object<TrackerStatusDto>> trackersDtoFromTorrent(const std::shared_ptr<lh::Torrent> &torrent) {
    auto list = oatpp::List<oatpp::Object<TrackerStatusDto>>::createShared();
    for (const auto &t : torrent->trackers_status()) {
        auto dto = TrackerStatusDto::createShared();
        dto->url = t.url;
        dto->tier = t.tier;
        dto->status = t.status;
        dto->fails = t.fails;
        dto->seeds = t.seeds;
        dto->peers = t.peers;
        dto->downloaded = t.downloaded;
        dto->next_announce = t.next_announce;
        dto->message = t.message;
        list->push_back(dto);
    }
    return list;
}

inline oatpp::Object<PiecesStatusDto> piecesDtoFromTorrent(const std::shared_ptr<lh::Torrent> &torrent) {
    auto pieces = torrent->pieces_status();
    auto dto = PiecesStatusDto::createShared();
    dto->map = pieces.map;
    dto->total = pieces.total;
    dto->stored = pieces.stored;
    dto->downloading = pieces.downloading;
    dto->prioritized = pieces.prioritized;
    dto->readers = pieces.readers;
    return dto;
}
