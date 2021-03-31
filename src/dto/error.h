#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

class ErrorDto : public oatpp::DTO {

  DTO_INIT(ErrorDto, DTO)

  DTO_FIELD(String, error);

};

#include OATPP_CODEGEN_END(DTO)
