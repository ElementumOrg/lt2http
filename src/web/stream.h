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

#include <utils/path.h>
#include <utils/requests.h>
#include <utils/strings.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

class StreamController : public oatpp::web::server::api::ApiController {
  public:
    explicit StreamController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<StreamController> createShared(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<StreamController>(objectMapper);
    }

    ENDPOINT_INFO(stream) {
        info->summary = "Stream file from torrent, identified by InfoHash";

        info->pathParams.add<String>("infoHash").description = "Torrent InfoHash";
        info->pathParams.add<String>("index").description = "File index";

        info->addResponse<oatpp::swagger::Binary>(Status::CODE_206, "application/octet-stream");
        // info->addResponse<oatpp::swagger::Binary>(Status::CODE_206, "application/json");
        info->addResponse<Object<FileOperationDto>>(Status::CODE_500, "application/json");
    }
    ENDPOINT("GET", "/torrents/{infoHash}/files/{index}/stream/{file_name}", stream, PATH(String, hash_param, "infoHash"),
             PATH(String, index_param, "index"),
             REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        auto hash = uri_unescape(hash_param->std_str());
        auto index = -1;

        for (const auto &header : request->getHeaders().getAll()) {
            OATPP_LOGD("FilesController::stream", "Header: %s = %s", header.first.std_str().c_str(),
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

            auto reader = std::make_shared<lh::Reader>(torrent, file, range);
            file->register_reader(reader->id(), reader.get());
            torrent->register_reader(reader->id(), reader.get());
            
            auto body = std::make_shared<oatpp::web::protocol::http::outgoing::StreamingBody>(reader);
            auto response = OutgoingResponse::createShared(Status::CODE_206, body);

            oatpp::web::protocol::http::ContentRange contentRange(oatpp::web::protocol::http::ContentRange::UNIT_BYTES,
                                                                  range.start, range.end, file->size(), true);

            response->putHeader("Accept-Ranges", "bytes");
            response->putHeader(Header::CONNECTION, Header::Value::CONNECTION_KEEP_ALIVE);
            response->putHeader(Header::CONTENT_LENGTH, std::to_string(file->size() - range.start).c_str());
            response->putHeader(Header::CONTENT_RANGE, contentRange.toString());

            auto mimeType = guess_mime_type(file->name());
            if (!mimeType.empty())
                response->putHeader(Header::CONTENT_TYPE, mimeType.c_str());

            OATPP_LOGD("FilesController::stream", "%s=%s", Header::CONTENT_LENGTH, std::to_string(file->size()).c_str())
            OATPP_LOGD("FilesController::stream", "%s=%s", Header::CONTENT_RANGE, contentRange.toString()->c_str())

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
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController) //<- End Codegen
