#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include "bittorrent/file.h"

#include OATPP_CODEGEN_BEGIN(DTO)

class FileStatusDto : public oatpp::DTO {

    DTO_INIT(FileStatusDto, DTO)

    DTO_FIELD_INFO(total) {
        info->description = "Total selected for download (bytes)";
    }
    DTO_FIELD(Int64, total);

    DTO_FIELD_INFO(total_done) {
        info->description = "Total downloaded (bytes)";
    }
    DTO_FIELD(Int64, total_done);

    DTO_FIELD_INFO(progress) {
        info->description = "Download progress in libtorrent";
    }
    DTO_FIELD(Float64, progress);

    DTO_FIELD_INFO(priority) {
        info->description = "File priority in libtorrent";
    }
    DTO_FIELD(Int8, priority);

    DTO_FIELD_INFO(buffering_total) {
        info->description = "Buffer size (bytes)";
    }
    DTO_FIELD(Int8, buffering_total);

    DTO_FIELD_INFO(buffering_progress) {
        info->description = "Percent of a buffering progress (0 to 100)";
    }
    DTO_FIELD(Float64, buffering_progress);
};

#include OATPP_CODEGEN_END(DTO)

inline oatpp::Object<FileStatusDto> fileStatusDtoFromFile(const std::shared_ptr<lh::File>& file, std::vector<std::int64_t> completed, std::vector<double> progress) {
    auto dto = FileStatusDto::createShared();

    dto->total = file->size();
    dto->total_done = completed[file->index()];
    dto->progress = progress[file->index()];
    dto->priority = file->priority();

    return dto;
}