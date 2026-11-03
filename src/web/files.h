#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include <libtorrent/download_priority.hpp>

#include <app/application.h>

#include <dto/file_info.h>
#include <dto/file_operation.h>
#include <dto/file_status.h>

#include "web/controller.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

class FilesController : public lh::Controller {
  public:
    explicit FilesController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper)) : lh::Controller(objectMapper) {}

    static std::shared_ptr<FilesController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<FilesController>(objectMapper);
    }

    ENDPOINT_INFO(download) {
        info->summary = "Download file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->queryParams.add<String>("buffer").description =
            "Define whether we need to start file buffering. Values: true/false. Default: false";
        info->queryParams.add<String>("buffer").required = false;

        info->addResponse<Object<FileOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/download", download, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"), 
            QUERY(String, buffer_param, "buffer", "false")
    ) {
        return fileOperation("FilesController::download", hash_param, index_param, [&](const std::string &hash, int index) {
            OATPP_LOGI("FilesController::download", "Downloading file index '%d' with torrent infohash: %s", index, hash.c_str())

            auto file = lh::session().get_torrent(hash)->get_file(index);
            file->set_priority(lt::low_priority);
            if (lh::is_true(buffer_param))
                file->start_buffer();

            return fileSuccess(hash, index, file->path());
        });
    }

    ENDPOINT_INFO(stop) {
        info->summary = "Stop download for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<Object<FileOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/stop", stop, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index")
    ) {
        return fileOperation("FilesController::stop", hash_param, index_param, [&](const std::string &hash, int index) {
            OATPP_LOGI("FilesController::stop", "Stop downloading file index '%d' with torrent infohash: %s", index, hash.c_str())

            auto file = lh::session().get_torrent(hash)->get_file(index);
            file->set_priority(lt::dont_download);

            return fileSuccess(hash, index, file->path());
        });
    }

    ENDPOINT_INFO(info) {
        info->summary = "Get info for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<Object<FileInfoDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/info", info, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"), 
            HEADER(String, hostHeader, Header::HOST)
    ) {
        return fileOperation("FilesController::info", hash_param, index_param, [&](const std::string &hash, int index) {
            auto file = lh::session().get_torrent(hash)->get_file(index);
            std::string host = "http://" + *hostHeader;
            return createDtoResponse(Status::CODE_200, fileInfoDtoFromFile(file, {}, {}, host, false));
        });
    }

    ENDPOINT_INFO(status) {
        info->summary = "Get status for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<Object<FileStatusDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/status", status, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index")
    ) {
        return fileOperation("FilesController::status", hash_param, index_param, [&](const std::string &hash, int index) {
            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            return createDtoResponse(Status::CODE_200,
                                     fileStatusDtoFromFile(file, torrent->files_completed_bytes(), torrent->files_progress()));
        });
    }
};

#include OATPP_CODEGEN_END(ApiController)
