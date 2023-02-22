#include <erd/erd.hpp>
#include <erd/ipc/message.hpp>

#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>
#include <fmt/format.h>

#include <unistd.h>

#include <cstdlib>
#include <iostream>

static std::string get_socket_path(bool unique) {
  auto get_socket_prefix = []() -> std::string {
    char *env = std::getenv("XDG_RUNTIME_DIR");
    if (!env)
      return "/tmp";
    return env;
  };

  if (char *env = std::getenv("ERD_SOCKET"); env) {
    return env;
  }
  std::string prefix = get_socket_prefix();
  if (unique) {
    return fmt::format("{}/{}-{}.sock", prefix, "erd", getpid());
  }
  return fmt::format("{}/{}.sock", prefix, "erd");
}

static bool process_message(const erd::reader_t &reader,
                            const erd::ipc::message_request &request,
                            erd::ipc::message_response &response,
                            std::error_code &ec) noexcept {
  using erd::ipc::operation_type_t;
  switch (request.operation_type()) {
  case operation_type_t::obtain_readings: {
    erd::ipc::status_code_t status = erd::ipc::status_code_t::success;
    erd::readings_t readings;
    if (!reader.obtain_readings(readings, ec)) {
      status = erd::ipc::status_code_t::error;
    }
    response.serialize(status, readings);
    return status == erd::ipc::status_code_t::success;
  }
  case operation_type_t::subtract: {
    erd::readings_t lhs;
    erd::readings_t rhs;
    if (request.readings(lhs, rhs, ec)) {
      response.serialize(erd::ipc::status_code_t::success,
                         reader.subtract(lhs, rhs));
      return true;
    } else {
      response.serialize(erd::ipc::status_code_t::error, erd::difference_t{});
      return false;
    }
  }
  }
  ec = std::make_error_code(std::errc::bad_message);
  return false;
}

int main() {
  std::string socket_path = get_socket_path(false);
  std::cout << socket_path << "\n";

  erd::reader_t reader{erd::attributes_t{erd::domain_t::package, 0}};

  asio::error_code ec;
  asio::io_context context;
  asio::local::stream_protocol::endpoint endpoint(socket_path);

  while (true) {
    ::unlink(socket_path.c_str());
    asio::local::stream_protocol::socket socket(context);
    asio::local::stream_protocol::acceptor acceptor(context, endpoint);

    acceptor.accept(socket, ec);
    if (ec) {
      std::cerr << "Error binding to socket: " << ec.message() << "\n";
      return 1;
    }

    std::error_code ec;
    erd::ipc::message_request request;
    erd::ipc::message_response response;
    while (true) {
      size_t bytes_read =
          socket.read_some(asio::buffer(request.buffer(), request.size), ec);
      if (ec) {
        break;
      }
      assert(bytes_read == request.size);
      if (!process_message(reader, request, response, ec)) {
        std::cerr << "Error processing message: " << ec.message() << "\n";
      }
      size_t bytes_written =
          socket.write_some(asio::buffer(response.buffer(), response.size), ec);
      if (ec) {
        break;
      }
      assert(bytes_written == response.size);
    }
  }
}
