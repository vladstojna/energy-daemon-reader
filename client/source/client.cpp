#include "client.hpp"

#include <cassert>

namespace erd::ipc {

reader_client::reader_client(std::string_view socket_path) : socket_(context_) {
  socket_.connect(asio::local::stream_protocol::endpoint(socket_path));
}

bool reader_client::obtain_readings(readings_t &into,
                                    std::error_code &ec) noexcept {
  request_.serialize();
  if (!comm_common(ec)) {
    return false;
  }
  assert(response_.operation_type() == operation_type_t::obtain_readings);
  if (response_.status_code() != status_code_t::success) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  return response_.readings(into, ec);
}

bool reader_client::subtract(difference_t &into, const readings_t &lhs,
                             const readings_t &rhs,
                             std::error_code &ec) noexcept {

  request_.serialize(lhs, rhs);
  if (!comm_common(ec)) {
    return false;
  }
  assert(response_.operation_type() == operation_type_t::subtract);
  if (response_.status_code() != status_code_t::success) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  return response_.difference(into, ec);
}

bool reader_client::comm_common(std::error_code &ec) noexcept {
  size_t bytes;
  bytes = socket_.write_some(
      asio::buffer(request_.buffer(), erd::ipc::message_request::size), ec);
  if (ec) {
    return false;
  }
  assert(bytes == request_.size);
  bytes = socket_.read_some(
      asio::buffer(response_.buffer(), erd::ipc::message_response::size), ec);
  if (ec) {
    return false;
  }
  assert(bytes == response_.size);
  ec.clear();
  return true;
}

} // namespace erd::ipc
