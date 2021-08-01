#pragma once

#include "oatpp/web/protocol/http/outgoing/Body.hpp"
#include "oatpp/web/protocol/http/Http.hpp"

namespace oatpp { namespace web { namespace protocol { namespace http { namespace outgoing {

class EmptyBody : public Body {
private:
  v_int64 m_size;
public:
  EmptyBody(const v_int64& sz);

  v_io_size read(void *buffer, v_buff_size count, async::Action& action) override;

  void declareHeaders(Headers& headers) override;

  p_char8 getKnownData() override;

  v_int64 getKnownSize() override;
  
};
  
}}}}}