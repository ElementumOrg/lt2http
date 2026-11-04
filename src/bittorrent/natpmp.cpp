#include "natpmp.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <future>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <libtorrent/enum_net.hpp>

#include <oatpp/core/base/Environment.hpp>

#include <utils/numbers.h>
#include <utils/strings.h>

namespace lh {

using boost::asio::ip::address_v4;
using boost::asio::ip::udp;

namespace {

constexpr unsigned short natpmp_port = 5351;
// Renewed every 45 seconds, a short lifetime lets the gateway drop mappings of a crashed process quickly
constexpr int mapping_lifetime = 60;

struct Response {
    bool ok = false;
    bool responded = false;
    int internal = 0;
    int external = 0;
    std::string error;
};

// One mapping request, resent after 250, 500 and 750ms (1.5s in total) while there is no answer.
Response request(const address_v4 &local, const address_v4 &gateway, bool tcp, int internal, int external, int lifetime, int attempts,
                 const std::atomic<bool> &abort) {
    Response ret;
    boost::asio::io_context ioc;
    udp::socket socket(ioc);
    boost::system::error_code ec;
    socket.open(udp::v4(), ec);
    if (!ec)
        socket.bind(udp::endpoint(local, 0), ec);
    if (ec) {
        ret.error = ec.message();
        return ret;
    }

    auto const opcode = static_cast<unsigned char>(tcp ? 2 : 1);
    std::array<unsigned char, 12> req{0,
                                      opcode,
                                      0,
                                      0,
                                      static_cast<unsigned char>(internal >> 8),
                                      static_cast<unsigned char>(internal),
                                      static_cast<unsigned char>(external >> 8),
                                      static_cast<unsigned char>(external),
                                      static_cast<unsigned char>(lifetime >> 24),
                                      static_cast<unsigned char>(lifetime >> 16),
                                      static_cast<unsigned char>(lifetime >> 8),
                                      static_cast<unsigned char>(lifetime)};
    std::array<unsigned char, 16> res{};
    udp::endpoint const target(gateway, natpmp_port);
    udp::endpoint sender;
    bool done = false;

    std::function<void(const boost::system::error_code &, std::size_t)> on_receive = [&](const boost::system::error_code &e,
                                                                                         std::size_t size) {
        if (e) {
            // Windows reports an ICMP port unreachable from the gateway here
            ret.error = e.message();
            done = true;
            return;
        }
        if (sender != target || size < res.size() || res[0] != 0 || res[1] != 128 + opcode) {
            socket.async_receive_from(boost::asio::buffer(res), sender, on_receive);
            return;
        }

        ret.responded = done = true;
        if (int const code = res[2] << 8 | res[3]) {
            ret.error = Fmt("result code %d", code);
            return;
        }
        ret.ok = true;
        ret.internal = res[8] << 8 | res[9];
        ret.external = res[10] << 8 | res[11];
    };
    socket.async_receive_from(boost::asio::buffer(res), sender, on_receive);

    for (int i = 0, timeout = 250; i < attempts && !done && !abort; i++, timeout += 250) {
        socket.send_to(boost::asio::buffer(req), target, 0, ec);
        if (ec) {
            ret.error = ec.message();
            return ret;
        }
        ioc.run_for(std::chrono::milliseconds(timeout));
    }
    if (!done)
        ret.error = "no response";
    return ret;
}

void delete_port(const address_v4 &local, const address_v4 &gateway, int port) {
    // Best effort: a single short attempt, the mapping expires with its lifetime anyway
    std::atomic<bool> const never{false};
    for (bool tcp : {true, false})
        request(local, gateway, tcp, port, port, 0, 1, never);
}

// Maps the port for TCP and UDP, ok only if the gateway kept the internal port as the external one.
Response map_port(const address_v4 &local, const address_v4 &gateway, int port, const std::atomic<bool> &abort) {
    auto tcp = request(local, gateway, true, port, port, mapping_lifetime, 3, abort);
    if (!tcp.ok) {
        tcp.error = "TCP mapping: " + tcp.error;
        return tcp;
    }
    auto udp = request(local, gateway, false, port, port, mapping_lifetime, 3, abort);
    if (!udp.ok) {
        udp.error = "UDP mapping: " + udp.error;
        return udp;
    }

    if (tcp.internal != tcp.external || udp.internal != udp.external) {
        // Unusable for us, release it instead of leaving it until the lifetime expires
        delete_port(local, gateway, tcp.internal);
        tcp.ok = false;
        tcp.error = Fmt("internal (%d) and external (%d) ports do not match", tcp.internal, tcp.external);
        return tcp;
    }
    if (tcp.internal != udp.internal)
        OATPP_LOGD("NatPmp", "TCP (%d) and UDP (%d) ports do not match, using TCP", tcp.internal, udp.internal)
    return tcp;
}

bool is_private(const address_v4 &a) {
    auto const ip = a.to_uint();
    return (ip >> 24) == 10 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8;
}

} // namespace

std::vector<address_v4> natpmp_gateway_guesses(const address_v4 &local, const address_v4 &route_gateway) {
    std::vector<address_v4> ret;
    auto add = [&](const address_v4 &gw) {
        if (!gw.is_unspecified() && gw != local && std::find(ret.begin(), ret.end(), gw) == ret.end())
            ret.push_back(gw);
    };
    add(route_gateway);
    for (int bits : {24, 16, 8})
        add(address_v4((local.to_uint() & (0xFFFFFFFFu << (32 - bits))) + 1));
    return ret;
}

std::vector<NatMapping> natpmp_map(const std::vector<std::string> &interfaces, int port_min, int port_max, const std::atomic<bool> &abort) {
    boost::asio::io_context ios;
    lt::error_code ec;
    auto const ifaces = lt::enum_net_interfaces(ios, ec);
    if (ec) {
        OATPP_LOGW("NatPmp", "Cannot list network interfaces: %s", ec.message().c_str())
        return {};
    }
    auto const routes = lt::enum_routes(ios, ec);

    // Local IPv4 addresses behind each listen interface: device names and IPs, or all private ones for the wildcard.
    // IPv6 entries are skipped, NAT-PMP maps IPv4 only.
    std::vector<std::pair<NatMapping, std::vector<address_v4>>> candidates;
    for (const auto &entry : interfaces) {
        for (const auto &iface : ifaces) {
            if (!iface.interface_address.is_v4() || iface.interface_address.is_loopback() || !(iface.flags & lt::if_flags::up))
                continue;
            auto const local = iface.interface_address.to_v4();
            bool const loopback = bool(iface.flags & lt::if_flags::loopback);
            if (entry == "0.0.0.0" ? loopback || !is_private(local) : entry != iface.name && entry != local.to_string())
                continue;
            if (std::any_of(candidates.begin(), candidates.end(), [&](const auto &c) { return c.first.local == local; }))
                continue;

            address_v4 route_gateway;
            if (auto gw = lt::get_gateway(iface, routes); gw && gw->is_v4())
                route_gateway = gw->to_v4();

            NatMapping m;
            m.entry = entry;
            m.local = local;
            m.port = random(port_min, port_max);
            candidates.emplace_back(m, natpmp_gateway_guesses(local, route_gateway));
        }
    }

    // Most probed gateways are guesses that do not answer, so all addresses are probed at once
    std::vector<std::future<NatMapping>> probes;
    for (auto &[candidate, gateways] : candidates) {
        probes.push_back(std::async(std::launch::async, [&abort, m = candidate, gateways = gateways]() mutable {
            std::vector<std::string> errors;
            for (const auto &gw : gateways) {
                auto r = map_port(m.local, gw, m.port, abort);
                // Some gateways do not let us pick the port, ask for any
                if (!r.ok && r.responded)
                    r = map_port(m.local, gw, 0, abort);
                if (r.ok) {
                    m.gateway = gw;
                    m.port = r.external;
                    OATPP_LOGI("NatPmp", "Mapped port %d for %s via gateway %s", m.port, m.local.to_string().c_str(),
                               gw.to_string().c_str())
                    return m;
                }
                errors.push_back(gw.to_string() + ": " + r.error);
            }
            OATPP_LOGD("NatPmp", "Not available for %s: %s", m.local.to_string().c_str(), join(errors, "; ").c_str())
            m.port = 0;
            return m;
        }));
    }

    std::vector<NatMapping> ret;
    for (auto &probe : probes)
        if (auto m = probe.get(); m.port > 0)
            ret.push_back(m);
    return ret;
}

bool natpmp_refresh(const NatMapping &mapping, const std::atomic<bool> &abort) {
    auto const r = map_port(mapping.local, mapping.gateway, mapping.port, abort);
    if (!r.ok)
        OATPP_LOGW("NatPmp", "Refresh of port %d for %s failed: %s", mapping.port, mapping.local.to_string().c_str(), r.error.c_str())
    else if (r.external != mapping.port)
        OATPP_LOGW("NatPmp", "Gateway changed port %d to %d for %s", mapping.port, r.external, mapping.local.to_string().c_str())
    return r.ok && r.external == mapping.port;
}

void natpmp_delete(const NatMapping &mapping) {
    OATPP_LOGI("NatPmp", "Deleting port mapping %d for %s", mapping.port, mapping.local.to_string().c_str())
    delete_port(mapping.local, mapping.gateway, mapping.port);
}

} // namespace lh
