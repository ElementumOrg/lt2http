#pragma once

#include "oatpp/web/protocol/http/outgoing/Body.hpp"

namespace oatpp { namespace web { namespace protocol { namespace http { namespace outgoing {

class StreamBody : public Body {
private:
  std::shared_ptr<data::stream::ReadCallback> m_readCallback;
  std::int64_t m_size;
public:

  StreamBody(const std::shared_ptr<data::stream::ReadCallback>& readCallback, const std::int64_t sz);

  v_io_size read(void *buffer, v_buff_size count, async::Action& action) override;

  void declareHeaders(Headers& headers) override;

  p_char8 getKnownData() override;

  v_buff_size getKnownSize() override;

};

}}}}}