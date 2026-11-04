#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <json_struct/json_struct.h>

#include <libtorrent/settings_pack.hpp>
#include <libtorrent/version.hpp>

#include <app/application.h>
#include <app/build_info.h>

#include OATPP_CODEGEN_BEGIN(ApiController)

class SettingsController : public oatpp::web::server::api::ApiController {
  public:
    explicit SettingsController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}

    static std::shared_ptr<SettingsController>
    createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper) // Inject objectMapper component here as default parameter
    ) {
        return std::make_shared<SettingsController>(objectMapper);
    }

    ENDPOINT_INFO(get) {
        info->summary = "Get global settings";

        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/settings/get", get) {
        auto response = createResponse(Status::CODE_200, oatpp::String(JS::serializeStruct(lh::config()).c_str()));
        response->putHeader(Header::CONTENT_TYPE, "application/json");
        return response;
    }

    ENDPOINT_INFO(set) {
        info->summary = "Set global settings";

        info->addConsumes<String>("application/json");
        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("POST", "/settings/set", set, BODY_STRING(String, body)) {
        lh::Config &config = lh::config();
        JS::ParseContext context(body->c_str());
        auto err = context.parseTo(config);

        OATPP_ASSERT_HTTP(err == JS::Error::NoError, Status::CODE_500, ("Error converting json: " + context.makeErrorString()).c_str())

        OATPP_LOGW("SettingsController::set", "Configuration updated to: %s", JS::serializeStruct(config).c_str())
        lh::session().reconfigure();

        auto response = createResponse(Status::CODE_200, oatpp::String(JS::serializeStruct(config).c_str()));
        response->putHeader(Header::CONTENT_TYPE, "application/json");
        return response;
    }

    ENDPOINT_INFO(state) {
        info->summary = "Get lt2http settings (without passwords), libtorrent settings, session and network state";

        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/settings/state", state) {
        using Fields = oatpp::Fields<oatpp::Any>;
        auto &session = lh::session();

        auto libtorrent = Fields::createShared();
        auto const pack = session.native_settings();
        auto add_settings = [&](int base, int count, auto value_of) {
            for (int i = base; i < base + count; i++) {
                const char *name = lt::name_for_setting(i);
                // Deprecated settings have no name
                if (name && *name)
                    libtorrent->push_back({name, value_of(i)});
            }
        };
        add_settings(lt::settings_pack::string_type_base, lt::settings_pack::num_string_settings, [&pack](int i) {
            // Credentials of the proxy
            bool const is_secret = i == lt::settings_pack::proxy_username || i == lt::settings_pack::proxy_password;
            return oatpp::Any(oatpp::String(is_secret && !pack.get_str(i).empty() ? "***" : pack.get_str(i)));
        });
        add_settings(lt::settings_pack::int_type_base, lt::settings_pack::num_int_settings,
                     [&pack](int i) { return oatpp::Any(oatpp::Int64(pack.get_int(i))); });
        add_settings(lt::settings_pack::bool_type_base, lt::settings_pack::num_bool_settings,
                     [&pack](int i) { return oatpp::Any(oatpp::Boolean(pack.get_bool(i))); });

        auto stats = Fields::createShared();
        for (const auto &[name, value] : session.session_stats())
            stats->push_back({name, oatpp::Any(oatpp::Int64(value))});

        auto state = Fields::createShared();
        state->push_back({"version", oatpp::Any(oatpp::String(lh::VERSION))});
        state->push_back({"commit", oatpp::Any(oatpp::String(lh::COMMIT))});
        state->push_back({"libtorrent_version", oatpp::Any(oatpp::String(LIBTORRENT_VERSION))});
        state->push_back({"user_agent", oatpp::Any(oatpp::String(session.get_user_agent()))});
        state->push_back({"is_paused", oatpp::Any(oatpp::Boolean(session.is_paused()))});
        state->push_back({"torrents_count", oatpp::Any(oatpp::Int64(std::int64_t(session.torrents().size())))});
        state->push_back({"listen_port", oatpp::Any(oatpp::Int64(session.listen_port()))});
        state->push_back({"is_dht_running", oatpp::Any(oatpp::Boolean(session.is_dht_running()))});
        state->push_back({"memory_size", oatpp::Any(oatpp::Int64(session.memory_size()))});
        state->push_back({"stats", oatpp::Any(stats)});

        auto net = session.network_state();
        auto list = [](const auto &items, auto to_fields) {
            auto result = oatpp::List<oatpp::Any>::createShared();
            for (const auto &item : items)
                result->push_back(oatpp::Any(to_fields(item)));
            return oatpp::Any(result);
        };
        auto fields = [](std::initializer_list<std::pair<const char *, oatpp::Any>> values) {
            auto result = Fields::createShared();
            for (const auto &[name, value] : values)
                result->push_back({name, value});
            return result;
        };
        auto string = [](const std::string &s) { return oatpp::Any(oatpp::String(s)); };
        auto number = [](int n) { return oatpp::Any(oatpp::Int32(n)); };
        auto network = fields({
            {"web_addresses", list(net.web_addresses, [&](const std::string &a) { return oatpp::String(a); })},
            {"web_port", number(net.web_port)},
            {"listen_interfaces", string(net.listen_interfaces)},
            {"listen_sockets",
             list(net.sockets,
                  [&](const lh::ListenSocket &s) {
                      return fields(
                          {{"address", string(s.address)}, {"port", number(s.port)}, {"type", string(s.type)}, {"error", string(s.error)}});
                  })},
            {"natpmp_enabled", oatpp::Any(oatpp::Boolean(net.natpmp_enabled))},
            {"natpmp_mappings", list(net.nat_mappings,
                                     [&](const lh::NatMapping &m) {
                                         return fields({{"interface", string(m.entry)},
                                                        {"local", string(m.local.to_string())},
                                                        {"gateway", string(m.gateway.to_string())},
                                                        {"port", number(m.port)}});
                                     })},
            {"port_mappings", list(net.port_mappings,
                                   [&](const lh::PortMapping &m) {
                                       return fields({{"transport", string(m.transport)},
                                                      {"local_port", number(m.local_port)},
                                                      {"message", string(m.message)},
                                                      {"is_error", oatpp::Any(oatpp::Boolean(m.is_error))}});
                                   })},
        });

        auto result = Fields::createShared();
        result->push_back({"lt2http", getDefaultObjectMapper()->readFromString<oatpp::Any>(JS::serializeStruct(lh::config().masked()))});
        result->push_back({"libtorrent", oatpp::Any(libtorrent)});
        result->push_back({"session", oatpp::Any(state)});
        result->push_back({"network", oatpp::Any(network)});
        return createDtoResponse(Status::CODE_200, result);
    }
};

#include OATPP_CODEGEN_END(ApiController)
