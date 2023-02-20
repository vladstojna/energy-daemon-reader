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

int main() {
  std::string socket_path = get_socket_path(false);

  std::cout << socket_path << "\n";
  asio::error_code ec;
  asio::io_context context;
  asio::local::stream_protocol::endpoint endpoint(socket_path);
  asio::local::stream_protocol::acceptor acceptor(context, endpoint);
  asio::local::stream_protocol::socket socket(context);

  acceptor.accept(socket, ec);
  if (!ec) {
    std::cerr << "Accepted!\n";
  } else {
    std::cerr << "Error binding to socket: " << ec.message() << "\n";
  }
}
