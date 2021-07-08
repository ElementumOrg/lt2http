#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <boost/algorithm/string.hpp>

#include <app/application.h>
#include <app/config.h>

#include <dto/error.h>
#include <dto/file_info.h>
#include <dto/torrent_add.h>
#include <dto/torrent_info.h>
#include <dto/torrent_operation.h>

#include <oatpp/core/Types.hpp>
#include <utils/path.h>
#include <utils/strings.h>

#include "web/basic_auth.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

class TorrentsController : public oatpp::web::server::api::ApiController {
  public:
    std::shared_ptr<AuthorizationHandler> m_authHandler = std::make_shared<lh::CustomBasicAuthorizationHandler>();

    explicit TorrentsController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<TorrentsController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<TorrentsController>(objectMapper);
    }

    ENDPOINT_INFO(list) {
        info->summary = "List torrents";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include torrent status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addSecurityRequirement("basic_auth");

        info->addResponse<List<Object<TorrentInfoDto>>>(Status::CODE_200, "application/json");
        info->addResponse<Object<ErrorDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents", list, 
        QUERY(String, status_param, "status", "false"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        try {
            auto is_status = uri_unescape(status_param->std_str()) == "true";

            auto list = oatpp::List<oatpp::Object<TorrentInfoDto>>::createShared();

            auto torrents = lh::session().torrents();
            for (auto &torrent : torrents) {
                list->push_back(torrentInfoDtoFromTorrent(torrent, is_status));
            }

            return createDtoResponse(Status::CODE_200, list);
            // OATPP_ASSERT_HTTP(res, Status::CODE_500, ("Error adding torrent"));
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::torrents", "Error getting torrents: %s", e.what())

            auto dto = ErrorDto::createShared();
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
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

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/remove", remove, 
        PATH(String, hash_param, "infoHash"),
        QUERY(String, delete_files_param, "deleteFiles", "true"), 
        QUERY(String, delete_data_param, "deleteData", "false"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());

        try {
            auto is_delete_files = uri_unescape(delete_files_param->std_str()) == "true";
            auto is_delete_data = uri_unescape(delete_data_param->std_str()) == "true";

            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::remove", "Removing torrent with infohash: %s", hash.c_str())

            lh::session().remove_torrent(hash, is_delete_files, is_delete_data);

            auto dto = TorrentOperationDto::createShared();
            dto->success = true;
            dto->hash = hash.c_str();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::remove", "Error removing torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(resume) {
        info->summary = "Resume torrent identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/resume", resume, 
        PATH(String, hash_param, "infoHash"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());

        try {
            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::resume", "Resuming torrent with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            torrent->resume();

            auto dto = TorrentOperationDto::createShared();
            dto->success = true;
            dto->hash = hash.c_str();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::resume", "Error resuming torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(pause) {
        info->summary = "Pause torrent identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/pause", pause, 
        PATH(String, hash_param, "infoHash"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());

        try {
            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::pause", "Pausing torrent with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            torrent->pause();

            auto dto = TorrentOperationDto::createShared();
            dto->success = true;
            dto->hash = hash.c_str();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::pause", "Error pausing torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(info) {
        info->summary = "Get info for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include torrent status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<TorrentInfoDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/info", info, 
        PATH(String, hash_param, "infoHash"),
        QUERY(String, status_param, "status", "false"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());
        auto is_status = uri_unescape(status_param->std_str()) == "true";

        try {
            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::info", "Getting torrent info with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);

            return createDtoResponse(Status::CODE_200, torrentInfoDtoFromTorrent(torrent, is_status));
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::info", "Error getting info for torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(status) {
        info->summary = "Get status for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<TorrentStatusDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/status", status, 
        PATH(String, hash_param, "infoHash"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());

        try {
            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::status", "Getting torrent status with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);

            return createDtoResponse(Status::CODE_200, torrentStatusDtoFromTorrent(torrent));
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::status", "Error getting status for torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(files) {
        info->summary = "Get files for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->queryParams.add<String>("status").description =
            "Define whether we need to include file status fields. Values: true/false. Default: false";
        info->queryParams.add<String>("status").required = false;

        info->addSecurityRequirement("basic_auth");

        info->addResponse<List<Object<FileInfoDto>>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files", files, 
        PATH(String, hash_param, "infoHash"),
        QUERY(String, status_param, "status", "false"), 
        HEADER(String, hostHeader, Header::HOST),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());
        auto is_status = uri_unescape(status_param->std_str()) == "true";

        try {
            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::files", "Getting torrent files with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);

            auto list = oatpp::List<oatpp::Object<FileInfoDto>>::createShared();
            auto completed = torrent->files_completed_bytes();
            auto progress = torrent->files_progress();
            std::string host = "http://" + hostHeader->std_str();

            for (auto &file : torrent->files()) {
                list->push_back(fileInfoDtoFromFile(file, completed, progress, host, is_status));
            }

            return createDtoResponse(Status::CODE_200, list);
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::files", "Error getting torrent files for torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(generate) {
        info->summary = "Generate torrent file for torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";

        info->addSecurityRequirement("basic_auth");

        // info->addResponse<Object<TorrentStatusDto>>(Status::CODE_200, "application/json");
        // info->addResponse<Object<TorrentOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/generate", generate, 
        PATH(String, hash_param, "infoHash"),
        AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());

        try {
            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("TorrentsController::generate", "Getting torrent file with infohash: %s", hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            auto tf = torrent->generate();

            return createResponse(Status::CODE_200, oatpp::String(tf.data()));
        } catch (std::exception &e) {
            OATPP_LOGE("TorrentsController::generate", "Error getting torrent file for torrent: %s", e.what())

            auto dto = TorrentOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
