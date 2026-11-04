#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "bittorrent/session.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class SessionStatusDto : public oatpp::DTO {

    DTO_INIT(SessionStatusDto, DTO)

    DTO_FIELD_INFO(torrents_count) { info->description = "Total number of active torrents"; }
    DTO_FIELD(Int64, torrents_count);

    DTO_FIELD_INFO(is_paused) { info->description = "Is a session paused (bool)"; }
    DTO_FIELD(Boolean, is_paused);

    DTO_FIELD_INFO(version) { info->description = "lt2http version"; }
    DTO_FIELD(String, version);

    DTO_FIELD_INFO(commit) { info->description = "Git commit lt2http is built from"; }
    DTO_FIELD(String, commit);

    DTO_FIELD_INFO(libtorrent_version) { info->description = "libtorrent version"; }
    DTO_FIELD(String, libtorrent_version);

    DTO_FIELD_INFO(libtorrent_revision) { info->description = "Git commit of libtorrent"; }
    DTO_FIELD(String, libtorrent_revision);

    DTO_FIELD_INFO(user_agent) { info->description = "User agent sent to peers and trackers"; }
    DTO_FIELD(String, user_agent);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::Object<SessionStatusDto> sessionStatusDtoFromSession(lh::Session &session) {
    auto dto = SessionStatusDto::createShared();

    dto->torrents_count = session.torrents().size();
    dto->is_paused = session.is_paused();
    dto->version = lh::VERSION;
    dto->commit = lh::COMMIT;
    dto->libtorrent_version = LIBTORRENT_VERSION;
    dto->libtorrent_revision = LIBTORRENT_REVISION;
    dto->user_agent = session.get_user_agent();

    return dto;
}