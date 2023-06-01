#pragma once
#include <erd/ipc/message.hpp>

#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>

namespace erd::ipc {

class reader_client {
public:
  explicit reader_client(std::string_view socket_path);

  bool obtain_readings(readings_t &into, std::error_code &ec) noexcept;

  bool subtract(difference_t &into, const readings_t &lhs,
                const readings_t &rhs, std::error_code &ec) noexcept;

private:
  bool comm_common(std::error_code &ec) noexcept;

  asio::io_context context_;
  asio::local::stream_protocol::socket socket_;
  message_request request_;
  message_response response_;
};

} // namespace erd::ipc
