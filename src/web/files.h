#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
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

    static std::shared_ptr<FilesController>
    createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
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
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/download", download, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"), QUERY(String, buffer_param, "buffer", "false")) {
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
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/stop", stop, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index")) {
        return fileOperation("FilesController::stop", hash_param, index_param, [&](const std::string &hash, int index) {
            OATPP_LOGI("FilesController::stop", "Stop downloading file index '%d' with torrent infohash: %s", index, hash.c_str())

            auto file = lh::session().get_torrent(hash)->get_file(index);
            file->set_priority(lt::dont_download);

            return fileSuccess(hash, index, file->path());
        });
    }

    ENDPOINT_INFO(buffer) {
        info->summary = "Buffer a part of a file from torrent, identified by InfoHash: downloads it first, with the end of the file";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->queryParams.add<String>("position").description = "Where the buffer starts in the file (bytes). Default: 0";
        info->queryParams.add<String>("position").required = false;

        info->queryParams.add<String>("length").description = "Buffer size (bytes). Default: buffer_size from the settings";
        info->queryParams.add<String>("length").required = false;

        info->addResponse<Object<FileOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/buffer", buffer, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"), QUERY(String, position_param, "position", "0"),
             QUERY(String, length_param, "length", "0")) {
        return fileOperation("FilesController::buffer", hash_param, index_param, [&](const std::string &hash, int index) {
            auto position = std::stoll(*position_param);
            auto length = std::stoll(*length_param);
            OATPP_LOGI("FilesController::buffer", "Buffering file index '%d' with torrent infohash: %s", index, hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            if (!torrent->is_memory_storage() && file->priority() < lt::low_priority)
                file->set_priority(lt::low_priority);
            file->start_buffer(position, length);

            return fileSuccess(hash, index, file->path());
        });
    }

    ENDPOINT_INFO(set_priority) {
        info->summary = "Set download priority for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->queryParams.add<String>("priority").description = "Priority from 0 (do not download) to 7 (highest), 4 is the default";

        info->addResponse<Object<FileOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/set_priority", set_priority, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"), QUERY(String, priority_param, "priority")) {
        return fileOperation("FilesController::set_priority", hash_param, index_param, [&](const std::string &hash, int index) {
            auto priority = lh::priority_from_param(priority_param);
            OATPP_LOGI("FilesController::set_priority", "Setting priority %d for file index '%d' with torrent infohash: %s", int(priority),
                       index, hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            lh::check_file_storage(*torrent);
            auto file = torrent->get_file(index);
            file->set_priority(priority);

            return fileSuccess(hash, index, file->path());
        });
    }

    ENDPOINT_INFO(download_all) {
        info->summary = "Download all files from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/download", download_all, PATH(String, hash_param, "infoHash")) {
        return torrentOperation("FilesController::download_all", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("FilesController::download_all", "Downloading all files with torrent infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            lh::check_file_storage(*torrent);
            torrent->files_priority(lt::low_priority);

            return torrentSuccess(hash);
        });
    }

    ENDPOINT_INFO(stop_all) {
        info->summary = "Stop download for all files from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/stop", stop_all, PATH(String, hash_param, "infoHash")) {
        return torrentOperation("FilesController::stop_all", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("FilesController::stop_all", "Stop downloading all files with torrent infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            lh::check_file_storage(*torrent);
            torrent->files_priority(lt::dont_download);

            return torrentSuccess(hash);
        });
    }

    ENDPOINT_INFO(info) {
        info->summary = "Get info for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include file status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addResponse<Object<FileInfoDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/info", info, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"), QUERY(String, status_param, "status", "false"), HEADER(String, hostHeader, Header::HOST)) {
        return fileOperation("FilesController::info", hash_param, index_param, [&](const std::string &hash, int index) {
            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            std::string host = "http://" + *hostHeader;
            if (!lh::is_true(status_param))
                return createDtoResponse(Status::CODE_200, fileInfoDtoFromFile(file, {}, {}, host, false));
            return createDtoResponse(Status::CODE_200,
                                     fileInfoDtoFromFile(file, torrent->files_completed_bytes(), torrent->files_progress(), host, true));
        });
    }

    ENDPOINT_INFO(status) {
        info->summary = "Get status for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<Object<FileStatusDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/status", status, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index")) {
        return fileOperation("FilesController::status", hash_param, index_param, [&](const std::string &hash, int index) {
            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            return createDtoResponse(Status::CODE_200,
                                     fileStatusDtoFromFile(file, torrent->files_completed_bytes(), torrent->files_progress()));
        });
    }
};

#include OATPP_CODEGEN_END(ApiController)
