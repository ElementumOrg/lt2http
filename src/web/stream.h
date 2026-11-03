#pragma once

#include <memory>

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp-swagger/Types.hpp"

#include <libtorrent/download_priority.hpp>

#include <app/application.h>

#include <bittorrent/reader.h>

#include <dto/file_operation.h>

#include <utils/empty_body.h>
#include <utils/requests.h>
#include <utils/stream_body.h>

#include "web/controller.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

class StreamController : public lh::Controller {
  private:
    Response streamFile(const oatpp::String &hash_param, const oatpp::String &index_param,
                        const std::shared_ptr<IncomingRequest> &request) {
        OATPP_LOGD("StreamController::stream", "Request Header: Method = %s", request->getStartingLine().method.toString()->c_str())
        for (const auto &header : request->getHeaders().getAll()) {
            OATPP_LOGD("StreamController::stream", "Request Header: %s = %s", header.first.std_str().c_str(),
                       header.second.std_str().c_str())
        }

        return fileOperation("StreamController::stream", hash_param, index_param, [&](const std::string &hash, int index) {
            auto torrent = lh::session().get_torrent(hash);
            auto file = torrent->get_file(index);

            // Ensure file is set for download for non-memory storage.
            if (!torrent->is_memory_storage() && file->priority() < lt::low_priority) {
                file->set_priority(lt::low_priority);
            }

            auto rangeHeader = request->getHeader(Header::RANGE);
            if (!rangeHeader || rangeHeader->empty())
                rangeHeader = "bytes=0-";
            auto range = oatpp::web::protocol::http::Range::parse(rangeHeader);
            if (range.end == 0 || range.end >= file->size())
                range.end = file->size() - 1;
            if (range.start < 0 || range.start > range.end) {
                auto response = createResponse(Status::CODE_416, "");
                response->putHeader(Header::CONTENT_RANGE, "bytes */" + std::to_string(file->size()));
                return response;
            }

            OATPP_LOGD("StreamController::stream", "Range: start=%s, end=%s", std::to_string(range.start).c_str(),
                       std::to_string(range.end).c_str())

            auto reader = std::make_shared<lh::Reader>(torrent, file, range);
            torrent->register_reader(reader.get());

            Response response;
            if (request->getStartingLine().method.toString() == "HEAD") {
                auto body = std::make_shared<oatpp::web::protocol::http::outgoing::EmptyBody>(file->size());
                response = OutgoingResponse::createShared(Status::CODE_200, body);
            } else {
                auto body = std::make_shared<oatpp::web::protocol::http::outgoing::StreamBody>(reader, range.end - range.start + 1);
                response = OutgoingResponse::createShared(Status::CODE_206, body);

                oatpp::web::protocol::http::ContentRange contentRange(oatpp::web::protocol::http::ContentRange::UNIT_BYTES,
                                                                      range.start, range.end, file->size(), true);
                response->putHeader(Header::CONTENT_RANGE, contentRange.toString());
            }

            response->putHeader("Accept-Ranges", "bytes");
            response->putHeader(Header::CONNECTION, Header::Value::CONNECTION_CLOSE);

            auto mimeType = guess_mime_type(file->name());
            if (!mimeType.empty())
                response->putHeader(Header::CONTENT_TYPE, mimeType);

            for (const auto &header : response->getHeaders().getAll()) {
                OATPP_LOGD("StreamController::stream", "Response Header: %s = %s", header.first.std_str().c_str(),
                           header.second.std_str().c_str())
            }

            return response;
        });
    }

  public:
    explicit StreamController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper)) : lh::Controller(objectMapper) {}

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
    ENDPOINT("HEAD", "/torrents/{infoHash}/files/{index}/stream/{file_name}", stream_head, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"),
            REQUEST(std::shared_ptr<IncomingRequest>, request)
    ) {
        return streamFile(hash_param, index_param, request);
    }

    ENDPOINT_INFO(stream) {
        info->summary = "Stream file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<oatpp::swagger::Binary>(Status::CODE_206, "application/octet-stream");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/stream/{file_name}", stream, 
            PATH(String, hash_param, "infoHash"),
            PATH(String, index_param, "index"),
            REQUEST(std::shared_ptr<IncomingRequest>, request)
    ) {
        return streamFile(hash_param, index_param, request);
    }
};

#include OATPP_CODEGEN_END(ApiController)
