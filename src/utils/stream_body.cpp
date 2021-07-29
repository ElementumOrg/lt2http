#include "stream_body.h"

namespace oatpp { namespace web { namespace protocol { namespace http { namespace outgoing {

StreamBody::StreamBody(const std::shared_ptr<data::stream::ReadCallback>& readCallback, const std::int64_t sz)
  : m_readCallback(readCallback),
    m_size(sz)
{}

v_io_size StreamBody::read(void *buffer, v_buff_size count, async::Action& action) {
  return m_readCallback->read(buffer, count, action);
}

void StreamBody::declareHeaders(Headers& headers) {
  (void) headers;
  // DO NOTHING
}

p_char8 StreamBody::getKnownData() {
  return nullptr;
}


v_int64 StreamBody::getKnownSize() {
  return m_size;
}

}}}}}