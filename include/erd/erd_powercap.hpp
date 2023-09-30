#pragma once

#include <erd/erd_common.hpp>
#include <erd/units.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>

namespace erd {

namespace detail {

class file_descriptor {
  int value_;

public:
  explicit file_descriptor(const std::string &path);
  explicit file_descriptor(const char *path);
  ~file_descriptor() noexcept;

  file_descriptor(const file_descriptor &fd);
  file_descriptor(file_descriptor &&fd) noexcept;
  file_descriptor &operator=(const file_descriptor &other);
  file_descriptor &operator=(file_descriptor &&other) noexcept;

  explicit operator int() const noexcept;
};

} // namespace detail

class reader_t {
public:
  explicit reader_t(attributes_t attr);

  bool obtain_readings(readings_t &into, std::error_code &ec) const noexcept;

  [[nodiscard]] difference_t subtract(const readings_t &lhs,
                                      const readings_t &rhs) const noexcept;

  [[nodiscard]] const attributes_t &attributes() const noexcept;

private:
  attributes_t attr_;
  detail::file_descriptor sensor_;
  energy_t maxvalue_;

  reader_t(attributes_t &&attr, const std::string &prefix);
};

} // namespace erd
