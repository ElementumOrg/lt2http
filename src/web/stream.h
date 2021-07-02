#pragma once

#include <memory>

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/protocol/http/outgoing/StreamingBody.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp-swagger/Types.hpp"

#include <boost/algorithm/string.hpp>

#include <libtorrent/download_priority.hpp>

#include <app/application.h>
#include <app/config.h>

#include <bittorrent/reader.h>

#include <dto/error.h>
#include <dto/file_info.h>
#include <dto/file_operation.h>
#include <dto/file_status.h>

#include <utils/empty_body.h>
#include <utils/stream_body.h>

#include <utils/path.h>
#include <utils/requests.h>
#include <utils/strings.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

class StreamController : public oatpp::web::server::api::ApiController {
  private:
    std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> streamFile(const oatpp::String& hash_param, const oatpp::String& index_param, 
        const std::shared_ptr<IncomingRequest>& request) {
        auto hash = uri_unescape(hash_param->std_str());
        auto index = -1;

        OATPP_LOGD("FilesController::stream", "Request Header: Method = %s", request->getStartingLine().method.toString()->c_str())
        for (const auto &header : request->getHeaders().getAll()) {
            OATPP_LOGD("FilesController::stream", "Request Header: %s = %s", header.first.std_str().c_str(),
                       header.second.std_str().c_str())
        }

        try {
            index = std::stoi(uri_unescape(index_param->std_str()));

            boost::trim(hash);
            boost::to_lower(hash);

            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);

            auto rangeHeader = request->getHeader(Header::RANGE);
            if (rangeHeader == nullptr || rangeHeader->getSize() == 0)
                rangeHeader = "bytes=0-";
            auto range = oatpp::web::protocol::http::Range::parse(rangeHeader);
            if (range.end == 0)
                range.end = file->size() - 1;

            OATPP_LOGD("FilesController::stream", "Range: start=%s, end=%s", std::to_string(range.start).c_str(),
                       std::to_string(range.end).c_str())

            std::int64_t contentLength = file->size() - range.start;
            std::shared_ptr<OutgoingResponse> response = nullptr;

            auto reader = std::make_shared<lh::Reader>(torrent, file, range);
            file->register_reader(reader->id(), reader.get());
            torrent->register_reader(reader->id(), reader.get());
            
            if (request->getStartingLine().method.toString() == "HEAD") {
                auto body = std::make_shared<oatpp::web::protocol::http::outgoing::EmptyBody>(file->size());
                response = OutgoingResponse::createShared(Status::CODE_200, body);
            } else {
                auto body = std::make_shared<oatpp::web::protocol::http::outgoing::StreamBody>(reader, contentLength);
                response = OutgoingResponse::createShared(Status::CODE_206, body);

                oatpp::web::protocol::http::ContentRange contentRange(oatpp::web::protocol::http::ContentRange::UNIT_BYTES,
                                                                    range.start, range.end, file->size(), true);

                response->putHeader(Header::CONTENT_RANGE, contentRange.toString());
            }

            response->putHeader("Accept-Ranges", "bytes");
            response->putHeader(Header::CONNECTION, Header::Value::CONNECTION_KEEP_ALIVE);

            auto mimeType = guess_mime_type(file->name());
            if (!mimeType.empty())
                response->putHeader(Header::CONTENT_TYPE, mimeType.c_str());

            for (const auto &header : response->getHeaders().getAll()) {
                OATPP_LOGD("FilesController::stream", "Response Header: %s = %s", header.first.std_str().c_str(),
                        header.second.std_str().c_str())
            }

            return response;
        } catch (std::exception &e) {
            OATPP_LOGE("FilesController::stream", "Error getting stream from file: %s", e.what())

            auto dto = FileOperationDto::createShared();
            dto->hash = hash.c_str();
            dto->id = index;
            dto->success = false;
            dto->error = e.what();

            return createDtoResponse(Status::CODE_500, dto);
        }

    };

  public:
    explicit StreamController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<StreamController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<StreamController>(objectMapper);
    }

    ENDPOINT_INFO(stream_head) {
        info->summary = "Stream file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<oatpp::swagger::Binary>(Status::CODE_200, "application/octet-stream");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("HEAD", "/torrents/{infoHash}/files/{index}/stream/{file_name}", stream_head, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"),
             REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        return streamFile(hash_param, index_param, request);
    }

    ENDPOINT_INFO(stream) {
        info->summary = "Stream file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<oatpp::swagger::Binary>(Status::CODE_206, "application/octet-stream");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/stream/{file_name}", stream, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"),
             REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        return streamFile(hash_param, index_param, request);
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
