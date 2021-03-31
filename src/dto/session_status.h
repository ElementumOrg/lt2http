#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include "bittorrent/session.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class SessionStatusDto : public oatpp::DTO {

    DTO_INIT(SessionStatusDto, DTO)

    DTO_FIELD_INFO(torrents_count) {
        info->description = "Total number of active torrents";
    }
    DTO_FIELD(Int64, torrents_count);

    DTO_FIELD_INFO(is_paused) {
        info->description = "Is a session paused (bool)";
    }
    DTO_FIELD(Boolean, is_paused);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::Object<SessionStatusDto> sessionStatusDtoFromSession(lh::Session& session) {
    auto dto = SessionStatusDto::createShared();

    dto->torrents_count = session.torrents().size();
    dto->is_paused = session.is_paused();

    return dto;
}