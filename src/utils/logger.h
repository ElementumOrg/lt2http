#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>

#include <oatpp/core/base/Environment.hpp>

namespace lh {

class CustomLogger : public oatpp::base::Logger {
  private:
    std::mutex m_lock;

  public:
    void log(v_uint32 priority, const std::string &tag, const std::string &message) override;
};

}