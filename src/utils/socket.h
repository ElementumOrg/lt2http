#pragma once

#include <cerrno>

#include <oatpp/core/IODefinitions.hpp>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#include <sys/socket.h>
#endif

// The peer closed or reset the connection: the socket is readable but has nothing to peek. Never blocks.
inline bool peer_closed(oatpp::v_io_handle handle) {
    char c;
#ifdef _WIN32
    WSAPOLLFD pfd{handle, POLLRDNORM, 0};
    if (WSAPoll(&pfd, 1, 0) <= 0)
        return false;
    return recv(handle, &c, 1, MSG_PEEK) <= 0;
#else
    pollfd pfd{handle, POLLIN, 0};
    if (poll(&pfd, 1, 0) <= 0)
        return false;
    auto const n = recv(handle, &c, 1, MSG_PEEK | MSG_DONTWAIT);
    return n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK);
#endif
}
