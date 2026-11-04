#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <boost/asio/ip/address_v4.hpp>

namespace lh {

// A NAT-PMP (RFC 6886) mapping of the same port for TCP and UDP, so libtorrent can listen on it.
struct NatMapping {
    std::string entry; // listen_interfaces entry the local address came from
    boost::asio::ip::address_v4 local;
    boost::asio::ip::address_v4 gateway;
    int port = 0;
};

// The gateway is unknown, especially behind a VPN, so for each IPv4 address behind the listen interfaces
// (all private addresses for a wildcard) the likely gateways are probed until one maps a port.
std::vector<NatMapping> natpmp_map(const std::vector<std::string> &interfaces, int port_min, int port_max, const std::atomic<bool> &abort);

// Renews the mapping, returns false if the gateway dropped it or moved it to another port.
bool natpmp_refresh(const NatMapping &mapping, const std::atomic<bool> &abort);

void natpmp_delete(const NatMapping &mapping);

// Gateways to probe for a local address: its route gateway, then the first address of its /24, /16 and /8.
std::vector<boost::asio::ip::address_v4> natpmp_gateway_guesses(const boost::asio::ip::address_v4 &local,
                                                                const boost::asio::ip::address_v4 &route_gateway);

} // namespace lh
