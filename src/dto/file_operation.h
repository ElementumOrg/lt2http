#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

class FileOperationDto : public oatpp::DTO {

  DTO_INIT(FileOperationDto, DTO)

  DTO_FIELD_INFO(success) {
    info->description = "Success bool";
  }
  DTO_FIELD(Boolean, success);

  DTO_FIELD_INFO(hash) {
    info->description = "Torrent info hash";
  }
  DTO_FIELD(String, hash);

  DTO_FIELD_INFO(id) {
    info->description = "File index id";
  }
  DTO_FIELD(Int32, id);

  DTO_FIELD_INFO(path) {
    info->description = "File path";
  }
  DTO_FIELD(String, path);

  DTO_FIELD_INFO(error) {
    info->description = "Error description";
  }
  DTO_FIELD(String, error);

};

#include OATPP_CODEGEN_END(DTO)
