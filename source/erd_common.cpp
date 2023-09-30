#include <erd/erd_common.hpp>

#include <iostream>
#include <system_error>

namespace {

auto default_error_handler_v =
    +[](const char *msg, std::error_code ec) noexcept {
      std::cerr << msg << " (" << ec << ")" << std::endl;
    };

auto default_output_v =
    +[](const char *msg) noexcept { std::cout << msg << "\n"; };

} // namespace


namespace erd {

void set_default_output(void (*func)(const char *) noexcept) noexcept {
  default_output_v = func;
}

void default_output(const char *msg) noexcept { default_output_v(msg); }

void set_default_error_handler(
    void (*func)(const char *, std::error_code) noexcept) noexcept {
  default_error_handler_v = func;
}

void default_error_handler(const char *msg, std::error_code ec) noexcept {
  return default_error_handler_v(msg, std::move(ec));
}

}
