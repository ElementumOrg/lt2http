#pragma once

#include <stdexcept>

namespace lh {

class Exception : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class ParseException : public Exception {
  public:
    using Exception::Exception;
};

class TorrentException : public Exception {
  public:
    using Exception::Exception;
};

class FileException : public Exception {
  public:
    using Exception::Exception;
};

class ReaderException : public Exception {
  public:
    using Exception::Exception;
};

} // namespace lh