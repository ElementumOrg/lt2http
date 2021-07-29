#include "empty_body.h"

namespace oatpp { namespace web { namespace protocol { namespace http { namespace outgoing {

EmptyBody::EmptyBody(const v_buff_size& sz)
  : m_size(sz)
{}

v_io_size EmptyBody::read(void *buffer, v_buff_size count, async::Action& action) {

  (void) action;

  return 0;

}

void EmptyBody::declareHeaders(Headers& headers) {
    (void) headers;
}

p_char8 EmptyBody::getKnownData() {
  return nullptr;
}

v_int64 EmptyBody::getKnownSize() {
  return m_size;
}

}}}}}