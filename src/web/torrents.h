#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include <app/application.h>

#include <dto/error.h>
#include <dto/file_info.h>
#include <dto/torrent_info.h>
#include <dto/torrent_operation.h>

#include "web/controller.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

class TorrentsController : public lh::Controller {
  public:
    explicit TorrentsController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper)) : lh::Controller(objectMapper) {}

    static std::shared_ptr<TorrentsController>
    createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<TorrentsController>(objectMapper);
    }

    ENDPOINT_INFO(list) {
        info->summary = "List torrents";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include torrent status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addResponse<List<Object<TorrentInfoDto>>>(Status::CODE_200, "application/json");
        info->addResponse<Object<ErrorDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents", list, QUERY(String, status_param, "status", "false")) {
        return operation<ErrorDto>("TorrentsController::list", [&] {
            auto is_status = lh::is_true(status_param);
            auto list = oatpp::List<oatpp::Object<TorrentInfoDto>>::createShared();
            for (const auto &torrent : lh::session().torrents())
                list->push_back(torrentInfoDtoFromTorrent(torrent, is_status));

            return createDtoResponse(Status::CODE_200, list);
        });
    }

    ENDPOINT_INFO(remove) {
        info->summary = "Remove torrent identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->queryParams.add<String>("deleteFiles").description =
            "Define whether we need to remove temporary files (.parts/.fastresume). Values: true/false. Default: true";
        info->queryParams.add<String>("deleteFiles").required = false;

        info->queryParams.add<String>("deleteData").description =
            "Define whether we need to remove downloaded files. Values: true/false. Default: false";
        info->queryParams.add<String>("deleteData").required = false;

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/remove", remove, PATH(String, hash_param, "infoHash"),
             QUERY(String, delete_files_param, "deleteFiles", "true"), QUERY(String, delete_data_param, "deleteData", "false")) {
        return torrentOperation("TorrentsController::remove", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::remove", "Removing torrent with infohash: %s", hash.c_str())
            lh::session().remove_torrent(hash, lh::is_true(delete_files_param), lh::is_true(delete_data_param));
            return torrentSuccess(hash);
        });
    }

    ENDPOINT_INFO(resume) {
        info->summary = "Resume torrent identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/resume", resume, PATH(String, hash_param, "infoHash")) {
        return torrentOperation("TorrentsController::resume", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::resume", "Resuming torrent with infohash: %s", hash.c_str())
            lh::session().get_torrent(hash)->resume();
            return torrentSuccess(hash);
        });
    }

    ENDPOINT_INFO(pause) {
        info->summary = "Pause torrent identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/pause", pause, PATH(String, hash_param, "infoHash")) {
        return torrentOperation("TorrentsController::pause", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::pause", "Pausing torrent with infohash: %s", hash.c_str())
            lh::session().get_torrent(hash)->pause();
            return torrentSuccess(hash);
        });
    }

    ENDPOINT_INFO(info) {
        info->summary = "Get info for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include torrent status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addResponse<Object<TorrentInfoDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/info", info, PATH(String, hash_param, "infoHash"),
             QUERY(String, status_param, "status", "false")) {
        return torrentOperation("TorrentsController::info", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::info", "Getting torrent info with infohash: %s", hash.c_str())
            auto torrent = lh::session().get_torrent(hash);
            return createDtoResponse(Status::CODE_200, torrentInfoDtoFromTorrent(torrent, lh::is_true(status_param)));
        });
    }

    ENDPOINT_INFO(status) {
        info->summary = "Get status for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addResponse<Object<TorrentStatusDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/status", status, PATH(String, hash_param, "infoHash")) {
        return torrentOperation("TorrentsController::status", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::status", "Getting torrent status with infohash: %s", hash.c_str())
            auto torrent = lh::session().get_torrent(hash);
            return createDtoResponse(Status::CODE_200, torrentStatusDtoFromTorrent(torrent));
        });
    }

    ENDPOINT_INFO(files) {
        info->summary = "Get files for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include file status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addResponse<List<Object<FileInfoDto>>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files", files, PATH(String, hash_param, "infoHash"),
             QUERY(String, status_param, "status", "false"), HEADER(String, hostHeader, Header::HOST)) {
        return torrentOperation("TorrentsController::files", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::files", "Getting torrent files with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            auto is_status = lh::is_true(status_param);
            auto completed = torrent->files_completed_bytes();
            auto progress = torrent->files_progress();
            std::string host = "http://" + *hostHeader;

            auto list = oatpp::List<oatpp::Object<FileInfoDto>>::createShared();
            for (const auto &file : torrent->files())
                list->push_back(fileInfoDtoFromFile(file, completed, progress, host, is_status));

            return createDtoResponse(Status::CODE_200, list);
        });
    }

    ENDPOINT_INFO(generate) {
        info->summary = "Generate torrent file for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
    }
    ENDPOINT("GET", "/torrents/{infoHash}/generate", generate, PATH(String, hash_param, "infoHash")) {
        return torrentOperation("TorrentsController::generate", hash_param, [&](const std::string &hash) {
            OATPP_LOGI("TorrentsController::generate", "Getting torrent file with infohash: %s", hash.c_str())
            auto tf = lh::session().get_torrent(hash)->generate();
            return createResponse(Status::CODE_200, oatpp::String(tf.data(), tf.size()));
        });
    }
};

#include OATPP_CODEGEN_END(ApiController)
