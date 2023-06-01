#include <erd/erd.hpp>
#include <erd/ipc/message.hpp>

#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>
#include <cxxopts.hpp>
#include <fmt/format.h>

#include <unistd.h>

#include <cstdlib>
#include <iostream>

static std::string get_socket_path(bool unique) {
  auto get_socket_prefix = []() -> std::string {
    char *env = std::getenv("XDG_RUNTIME_DIR");
    if (!env) {
      return "/tmp";
    }
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

static bool string_to_domain(std::string_view domain_str, erd::domain_t &domain,
                             std::error_code &ec) {
  bool retval = true;
  ec.clear();
  if (domain_str == "package") {
    domain = erd::domain_t::package;
  } else if (domain_str == "cores") {
    domain = erd::domain_t::cores;
  } else if (domain_str == "uncore") {
    domain = erd::domain_t::uncore;
  } else if (domain_str == "dram") {
    domain = erd::domain_t::dram;
  } else {
    ec = std::make_error_code(std::errc::invalid_argument);
    retval = false;
  }
  return retval;
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
    }
    response.serialize(erd::ipc::status_code_t::error, erd::difference_t{});
    return false;
  }
  }
  ec = std::make_error_code(std::errc::bad_message);
  return false;
}

int main(int argc, char *argv[]) {
  cxxopts::Options options("Energy reading daemon",
                           "Daemon that reads energy using the erd library");
  options.add_options() //
      ("u,unique", "Use unique socket name",
       cxxopts::value<bool>()->default_value("false")) //
      ("d,domain", "RAPL domain to read from - package, cores, uncore, dram",
       cxxopts::value<std::string>()->default_value("package")) //
      ("s,socket", "CPU socket to consider",
       cxxopts::value<uint32_t>()->default_value("0")) //
      ("h,help", "Print usage");
  auto result = options.parse(argc, argv);

  if (result.count("help") > 0) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  std::string socket_path = get_socket_path(result["unique"].as<bool>());

  std::error_code ec;
  erd::domain_t domain;
  std::string domain_str = result["domain"].as<std::string>();
  if (!string_to_domain(domain_str, domain, ec)) {
    std::cerr << "Invalid domain value: " << domain_str << "\n";
    return 1;
  }
  const uint32_t socket = result["socket"].as<uint32_t>();

  std::cout << "Socket path: " << socket_path << "\n";
  std::cout << "RAPL Domain: " << domain_str << "\n";
  std::cout << "CPU socket: " << socket << "\n";
  erd::reader_t reader{erd::attributes_t{domain, socket}};

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
      size_t bytes_read = socket.read_some(
          asio::buffer(request.buffer(), erd::ipc::message_request::size), ec);
      if (ec) {
        break;
      }
      assert(bytes_read == request.size);
      if (!process_message(reader, request, response, ec)) {
        std::cerr << "Error processing message: " << ec.message() << "\n";
      }
      size_t bytes_written = socket.write_some(
          asio::buffer(response.buffer(), erd::ipc::message_response::size),
          ec);
      if (ec) {
        break;
      }
      assert(bytes_written == response.size);
    }
  }
}
