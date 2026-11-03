#include "empty_body.h"

namespace oatpp::web::protocol::http::outgoing {

EmptyBody::EmptyBody(const v_int64 &sz) : m_size(sz) {}

v_io_size EmptyBody::read([[maybe_unused]] void *buffer, [[maybe_unused]] v_buff_size count, [[maybe_unused]] async::Action &action) {
    return 0;
}

void EmptyBody::declareHeaders([[maybe_unused]] Headers &headers) {}

p_char8 EmptyBody::getKnownData() { return nullptr; }

v_int64 EmptyBody::getKnownSize() { return m_size; }

} // namespace oatpp::web::protocol::http::outgoing