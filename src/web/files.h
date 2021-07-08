#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/protocol/http/outgoing/StreamingBody.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <boost/algorithm/string.hpp>

#include <libtorrent/download_priority.hpp>

#include <app/application.h>
#include <app/config.h>

#include <bittorrent/reader.h>

#include <dto/error.h>
#include <dto/file_info.h>
#include <dto/file_operation.h>
#include <dto/file_status.h>

#include <utils/path.h>
#include <utils/requests.h>
#include <utils/strings.h>

#include "web/basic_auth.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

class FilesController : public oatpp::web::server::api::ApiController {
  public:
    std::shared_ptr<AuthorizationHandler> m_authHandler = std::make_shared<lh::CustomBasicAuthorizationHandler>();

    explicit FilesController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

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

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<FileOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/download", download, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"), 
            QUERY(String, buffer_param, "buffer", "false"),
            AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());
        auto index = -1;

        try {
            auto is_buffer = uri_unescape(buffer_param->std_str()) == "true";
            index = std::stoi(uri_unescape(index_param->std_str()));

            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("FilesController::download", "Downloading file index '%d' with torrent infohash: %s", index, hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            file->set_priority(lt::low_priority);
            if (is_buffer)
                file->start_buffer();

            auto dto = FileOperationDto::createShared();
            dto->success = true;
            dto->hash = hash.c_str();
            dto->id = index;
            dto->path = file->path().c_str();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("FilesController::download", "Error downloading file: %s", e.what())

            auto dto = FileOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->id = index;
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(stop) {
        info->summary = "Stop download for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<FileOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/stop", stop, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"),
            AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());
        auto index = -1;

        try {
            index = std::stoi(uri_unescape(index_param->std_str()));

            boost::trim(hash);
            boost::to_lower(hash);

            OATPP_LOGI("FilesController::stop", "Stop downloading file index '%d' with torrent infohash: %s", index,
                       hash.c_str())

            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            file->set_priority(lt::dont_download);

            auto dto = FileOperationDto::createShared();
            dto->success = true;
            dto->hash = hash.c_str();
            dto->id = index;
            dto->path = file->path().c_str();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("FilesController::stop", "Error stopping download file: %s", e.what())

            auto dto = FileOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->id = index;
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(info) {
        info->summary = "Get info for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<FileInfoDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/info", info, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"), 
            HEADER(String, hostHeader, Header::HOST),
            AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());
        auto index = -1;

        try {
            index = std::stoi(uri_unescape(index_param->std_str()));

            boost::trim(hash);
            boost::to_lower(hash);

            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);

            std::string host = "http://" + hostHeader->std_str();
            return createDtoResponse(Status::CODE_200, fileInfoDtoFromFile(file, {}, {}, host, false));
        } catch (std::exception &e) {
            OATPP_LOGE("FilesController::info", "Error getting info from file: %s", e.what())

            auto dto = FileOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->id = index;
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(status) {
        info->summary = "Get status for file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addSecurityRequirement("basic_auth");

        info->addResponse<Object<FileStatusDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/status", status, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"),
            AUTHORIZATION(std::shared_ptr<lh::CustomAuthorizationObject>, authObject, m_authHandler)
    ) {
        if (authObject == nullptr) {
            return createResponse(Status::CODE_403, "");
        };

        auto hash = uri_unescape(hash_param->std_str());
        auto index = -1;

        try {
            index = std::stoi(uri_unescape(index_param->std_str()));

            boost::trim(hash);
            boost::to_lower(hash);

            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);
            auto completed = torrent->files_completed_bytes();
            auto progress = torrent->files_progress();

            return createDtoResponse(Status::CODE_200, fileStatusDtoFromFile(file, completed, progress));
        } catch (std::exception &e) {
            OATPP_LOGE("FilesController::status", "Error getting status from file: %s", e.what())

            auto dto = FileOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->id = index;
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
