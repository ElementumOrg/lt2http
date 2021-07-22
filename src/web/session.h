#pragma once

#include <sstream>

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/web/mime/multipart/FileStreamProvider.hpp"
#include "oatpp/web/mime/multipart/InMemoryPartReader.hpp"
#include "oatpp/web/mime/multipart/PartList.hpp"
#include "oatpp/web/mime/multipart/Reader.hpp"

#include <app/application.h>
#include <app/config.h>

#include <dto/error.h>
#include <dto/torrent_add.h>
#include <dto/session_operation.h>
#include <dto/session_status.h>

#include <utils/exceptions.h>
#include <utils/http_client.h>
#include <utils/path.h>
#include <utils/strings.h>

#include "web/basic_auth.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace multipart = oatpp::web::mime::multipart;

class SessionController : public oatpp::web::server::api::ApiController {
  public:
    explicit SessionController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<SessionController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<SessionController>(objectMapper);
    }

    ENDPOINT_INFO(info) {
        info->summary = "Dump information for all running torrents";

        info->addResponse<String>(Status::CODE_200, "text/plain");
    }
    ENDPOINT("GET", "/info", info) {
        std::stringstream ss;
        ss << "<!DOCTYPE html><html lang=\"en-US\"><head>"
            << "<title>lt2http Status</title>"
            << R"(<meta http-equiv="refresh" content="30">)"
            <<  "</head><body>";

        auto torrents = lh::session().torrents();
        for (auto &torrent : torrents) {
            ss << "<pre>";
            torrent->dump(ss);
            ss << "</pre>";
        }

        ss << "</body></html>";

        return createResponse(Status::CODE_200, ss.str().c_str());
    }

    ENDPOINT_INFO(status) {
        info->summary = "Show session status";

        info->addResponse<Object<SessionStatusDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<ErrorDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/status", status) {
        try {
            auto session = lh::session();
            return createDtoResponse(Status::CODE_200, sessionStatusDtoFromSession(session));
        } catch (std::exception &e) {
            OATPP_LOGE("SessionController::status", "Error getting status: %s", e.what())

            auto dto = ErrorDto::createShared();
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(resume) {
        info->summary = "Resume ression";

        info->addResponse<Object<SessionOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<SessionOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/resume", resume) {
        try {
            OATPP_LOGI("SessionController::resume", "Resuming session")

            lh::session().resume();

            auto dto = SessionOperationDto::createShared();
            dto->success = true;

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("SessionController::resume", "Error resuming torrent: %s", e.what())

            auto dto = SessionOperationDto::createShared();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(pause) {
        info->summary = "Pause session";

        info->addResponse<Object<SessionOperationDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<SessionOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/pause", pause) {
        try {
            OATPP_LOGI("SessionController::pause", "Pausing session")

            lh::session().pause();

            auto dto = SessionOperationDto::createShared();
            dto->success = true;

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("SessionController::pause", "Error pausing session: %s", e.what())

            auto dto = SessionOperationDto::createShared();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(add_file) {
        info->summary = "Add torrent by file (multipart form)";

        info->queryParams.add<String>("file").description = "Torrent file upload";
        info->queryParams.add<String>("file").required = true;

        info->queryParams.add<String>("storage").description =
            "Define which storage to use. Values: file/memory/automatic. Default: automatic (taken from confguration)";
        info->queryParams.add<String>("storage").required = false;

        info->addResponse<Object<TorrentAddDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentAddDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("POST", "/service/add/file", add_file, 
            QUERY(String, storage_param, "storage", "automatic"),
            REQUEST(std::shared_ptr<IncomingRequest>, request)
    ) {
        try {
            auto storage = lh::from_string<lh::storage_type_t, lh::js_storage_type_t_string_struct>(storage_param->c_str());
            auto multipart = std::make_shared<multipart::PartList>(request->getHeaders());
            multipart::Reader reader(multipart.get());
            reader.setDefaultPartReader(multipart::createInMemoryPartReader(10 * 1024 * 1024 /* max-data-size per part */));

            request->transferBody(&reader);

            auto file = multipart->getNamedPart("file");
            std::string uri = path_append(lh::config().torrents_path, "tmp_" + get_random_numeric(10) + ".torrent");

            OATPP_LOGI("SessionController::add_file", "Saving uploaded file to: %s", uri.c_str())

            save_file(uri, file->getInMemoryData()->c_str(), file->getInMemoryData()->getSize());

            auto torrent = lh::session().add_torrent(uri, false, storage, true, std::chrono::system_clock::now());

            auto dto = TorrentAddDto::createShared();
            dto->success = true;
            dto->hash = torrent->hash().c_str();
            dto->has_metadata = torrent->has_metadata();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("SessionController::add_file", "Error adding torrent: %s", e.what())

            auto dto = TorrentAddDto::createShared();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    ENDPOINT_INFO(add_uri) {
        info->summary = "Add torrent by providing magnet uri, uri to a torrent file or path to a local file";

        info->queryParams.add<String>("uri").description = "Magnet uri or path to local file or http(s) link";
        info->queryParams.add<String>("uri").required = true;

        info->queryParams.add<String>("storage").description =
            "Define which storage to use. Values: file/memory/automatic. Default: automatic (taken from confguration)";
        info->queryParams.add<String>("storage").required = false;

        info->addResponse<Object<TorrentAddDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<TorrentAddDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/service/add/uri", add_uri, 
            QUERY(String, uri_param, "uri", ""),
            QUERY(String, storage_param, "storage", "automatic")
    ) {
        try {
            auto uri = uri_unescape(uri_param->std_str());
            auto storage = lh::from_string<lh::storage_type_t, lh::js_storage_type_t_string_struct>(storage_param->c_str());

            if (uri.rfind("http", 0) == 0) {
                OATPP_LOGI("SessionController::add_uri", "Downloading torrent file from: %s", uri.c_str());

                auto res = do_request(uri);
                if (res == nullptr)
                    throw lh::Exception(Fmt("Could not download file from url '%s'. Response: null", uri.c_str()));
                if (res->status != 200)
                    throw lh::Exception(Fmt("Could not download file from url '%s'. Status: %d", uri.c_str(), res->status));

                uri = path_append(lh::config().torrents_path, "tmp_" + get_random_numeric(10) + ".torrent");

                OATPP_LOGI("SessionController::add_file", "Saving uploaded file to: %s", uri.c_str())

                save_file(uri, res->body.c_str(), res->body.size());
            }

            auto torrent = lh::session().add_torrent(uri, false, storage, true, std::chrono::system_clock::now());

            auto dto = TorrentAddDto::createShared();
            dto->success = true;
            dto->hash = torrent->hash().c_str();
            dto->has_metadata = torrent->has_metadata();

            return createDtoResponse(Status::CODE_200, dto);
        } catch (std::exception &e) {
            OATPP_LOGE("SessionController::add_uri", "Error adding torrent: %s", e.what())

            auto dto = TorrentAddDto::createShared();
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
