#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

class TorrentOperationDto : public oatpp::DTO {

  DTO_INIT(TorrentOperationDto, DTO)

  DTO_FIELD_INFO(success) {
    info->description = "Success bool";
  }
  DTO_FIELD(Boolean, success);

  DTO_FIELD_INFO(hash) {
    info->description = "Torrent info hash";
  }
  DTO_FIELD(String, hash);

  DTO_FIELD_INFO(error) {
    info->description = "Error description";
  }
  DTO_FIELD(String, error);

};

#include OATPP_CODEGEN_END(DTO)
