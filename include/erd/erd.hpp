#pragma once

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

#ifdef ERD_USE_SYSTEM_CLOCK
using clock_t = std::chrono::system_clock;
#else
using clock_t = std::chrono::steady_clock;
#endif

using time_point_t = std::chrono::time_point<clock_t>;
using energy_t = microjoules<uint64_t>;

enum class domain_t {
  package,
  uncore,
  cores,
  dram,
};

struct attributes_t {
  domain_t domain;
  std::uint32_t socket;
};

struct readings_t {
  time_point_t timestamp;
  energy_t energy;
};

struct difference_t {
  clock_t::duration duration;
  energy_t energy_consumed;
};

class reader_t {
private:
  attributes_t attr_;
  detail::file_descriptor sensor_;
  energy_t maxvalue_;

public:
  explicit reader_t(attributes_t attr);

  bool obtain_readings(readings_t &into, std::error_code &ec) const noexcept;

  difference_t subtract(const readings_t &lhs,
                        const readings_t &rhs) const noexcept;

  const attributes_t &attributes() const noexcept;

private:
  reader_t(attributes_t &&attr, const std::string &prefix);
};

void set_default_output(void (*func)(const char *) noexcept) noexcept;

void default_output(const char *msg) noexcept;

void set_default_error_handler(void (*func)(const char *,
                                            std::error_code) noexcept) noexcept;

void default_error_handler(const char *msg, std::error_code ec) noexcept;

} // namespace erd
