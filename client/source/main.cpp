#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>
#include <fmt/format.h>

#include <cstdlib>
#include <iostream>

static std::string get_socket_path() {
  auto get_socket_prefix = []() -> std::string {
    char *env = std::getenv("XDG_RUNTIME_DIR");
    if (!env)
      return "/tmp";
    return env;
  };

  if (char *env = std::getenv("ERD_SOCKET"); env) {
    return env;
  }
  return fmt::format("{}/{}.sock", get_socket_prefix(), "erd");
}

int main() {
  std::string socket_path = get_socket_path();

  std::cout << socket_path << "\n";
  asio::error_code ec;
  asio::io_context context;
  asio::local::stream_protocol::endpoint endpoint(socket_path);
  asio::local::stream_protocol::socket socket(context);

  socket.connect(endpoint, ec);
  if (!ec) {
    std::cerr << "Connected!\n";
  } else {
    std::cerr << "Error connecting to socket: " << ec.message() << "\n";
  }
}
