#pragma once

#include <utility>

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include "file_status.h"

#include "bittorrent/file.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class FileInfoDto : public oatpp::DTO {

    DTO_INIT(FileInfoDto, DTO)

    DTO_FIELD_INFO(id) {
        info->description = "File index";
    }
    DTO_FIELD(Int32, id);

    DTO_FIELD_INFO(size) {
        info->description = "Size of a file";
    }
    DTO_FIELD(Int64, size);

    DTO_FIELD_INFO(offset) {
        info->description = "Offset value (from torrent file)";
    }
    DTO_FIELD(Int64, offset);

    DTO_FIELD_INFO(path) {
        info->description = "File path (from torrent file)";
    }
    DTO_FIELD(String, path);

    DTO_FIELD_INFO(name) {
        info->description = "File name";
    }
    DTO_FIELD(String, name);

    DTO_FIELD_INFO(stream) {
        info->description = "Link to streaming";
    }
    DTO_FIELD(String, stream);

    DTO_FIELD(Object<FileStatusDto>, file_status);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::Object<FileInfoDto> fileInfoDtoFromFile(const std::shared_ptr<lh::File>& file, std::vector<std::int64_t> completed, std::vector<double> progress, std::string &host, bool is_status) {
    auto dto = FileInfoDto::createShared();

    dto->id = file->index();
    dto->size = file->size();
    dto->offset = file->offset();
    dto->path = file->path().c_str();
    dto->name = file->name().c_str();
    dto->stream = (host + file->stream_uri()).c_str();

    if (is_status)
        dto->file_status = fileStatusDtoFromFile(file, std::move(completed), std::move(progress));

    return dto;
}