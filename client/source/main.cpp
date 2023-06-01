#include "client.hpp"

#include <fmt/format.h>

#include <cstdlib>
#include <iostream>
#include <thread>

static std::string get_socket_path() {
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
  return fmt::format("{}/{}.sock", get_socket_prefix(), "erd");
}

int main() {
  std::string socket_path = get_socket_path();
  std::cout << socket_path << "\n";

  erd::ipc::reader_client reader{socket_path};
  erd::readings_t before;
  erd::readings_t after;

  if (std::error_code ec; !reader.obtain_readings(before, ec)) {
    std::cerr << "Error obtaining readings: " << ec.message() << "\n";
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds{1});
  if (std::error_code ec; !reader.obtain_readings(after, ec)) {
    std::cerr << "Error obtaining readings: " << ec.message() << "\n";
    return 1;
  }

  erd::difference_t diff;
  if (std::error_code ec; !reader.subtract(diff, after, before, ec)) {
    std::cerr << "Error subtracting readings: " << ec.message() << "\n";
    return 1;
  }

  std::cout << "Duration: " << diff.duration.count() << " ns\n";
  std::cout << "Energy: " << diff.energy_consumed.count() << " uJ\n";
}
