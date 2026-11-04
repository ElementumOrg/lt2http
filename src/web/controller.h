#pragma once

#include <string>
#include <type_traits>

#include <boost/algorithm/string.hpp>

#include "oatpp/web/server/api/ApiController.hpp"

#include <libtorrent/download_priority.hpp>

#include <bittorrent/torrent.h>

#include <dto/error.h>
#include <dto/file_operation.h>
#include <dto/torrent_operation.h>

#include <utils/strings.h>

namespace lh {

// Infohash from a path parameter: unescaped, trimmed and in lower case.
inline std::string hash_from_param(const oatpp::String &param) {
    auto hash = uri_unescape(*param);
    boost::trim(hash);
    boost::to_lower(hash);
    return hash;
}

inline bool is_true(const oatpp::String &param) { return uri_unescape(*param) == "true"; }

inline lt::download_priority_t priority_from_param(const oatpp::String &param) {
    auto priority = std::stoi(uri_unescape(*param));
    if (priority < int(lt::dont_download) || priority > int(lt::top_priority))
        throw std::invalid_argument("Priority must be from 0 to 7");
    return lt::download_priority_t(std::uint8_t(priority));
}

// Memory storage downloads only what is streamed or buffered, file priorities do nothing there
inline void check_file_storage(const Torrent &torrent) {
    if (torrent.is_memory_storage())
        throw std::invalid_argument("File priorities are not used with memory storage");
}

// Turns exceptions thrown by an endpoint into the 500 response DTOs that clients expect.
class Controller : public oatpp::web::server::api::ApiController {
  public:
    using ApiController::ApiController;

  protected:
    using Response = std::shared_ptr<OutgoingResponse>;

    // Runs action(hash), errors are reported as TorrentOperationDto.
    template <typename F> Response torrentOperation(const char *tag, const oatpp::String &hash_param, F &&action) {
        auto hash = hash_from_param(hash_param);
        try {
            return action(hash);
        } catch (const std::exception &e) {
            OATPP_LOGE(tag, "Error: %s", e.what())
            auto dto = TorrentOperationDto::createShared();
            dto->success = false;
            dto->hash = hash;
            dto->error = e.what();
            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    Response torrentSuccess(const std::string &hash) {
        auto dto = TorrentOperationDto::createShared();
        dto->success = true;
        dto->hash = hash;
        return createDtoResponse(Status::CODE_200, dto);
    }

    // Runs action(hash, index), errors are reported as FileOperationDto.
    template <typename F>
    Response fileOperation(const char *tag, const oatpp::String &hash_param, const oatpp::String &index_param, F &&action) {
        auto hash = hash_from_param(hash_param);
        int index = -1;
        try {
            index = std::stoi(uri_unescape(*index_param));
            return action(hash, index);
        } catch (const std::exception &e) {
            OATPP_LOGE(tag, "Error: %s", e.what())
            auto dto = FileOperationDto::createShared();
            dto->success = false;
            dto->hash = hash;
            dto->id = index;
            dto->error = e.what();
            return createDtoResponse(Status::CODE_500, dto);
        }
    }

    Response fileSuccess(const std::string &hash, int index, const std::string &path) {
        auto dto = FileOperationDto::createShared();
        dto->success = true;
        dto->hash = hash;
        dto->id = index;
        dto->path = path;
        return createDtoResponse(Status::CODE_200, dto);
    }

    // Runs action(), errors are reported as Dto with an error field and success = false (ErrorDto has no success field).
    template <typename Dto, typename F> Response operation(const char *tag, F &&action) {
        try {
            return action();
        } catch (const std::exception &e) {
            OATPP_LOGE(tag, "Error: %s", e.what())
            auto dto = Dto::createShared();
            if constexpr (!std::is_same_v<Dto, ErrorDto>)
                dto->success = false;
            dto->error = e.what();
            return createDtoResponse(Status::CODE_500, dto);
        }
    }
};

} // namespace lh
