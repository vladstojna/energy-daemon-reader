#pragma once

#include <erd/units.hpp>

#include <chrono>
#include <cstdint>
#include <system_error>

namespace erd {

void set_default_output(void (*func)(const char *) noexcept) noexcept;

void default_output(const char *msg) noexcept;

void set_default_error_handler(void (*func)(const char *,
                                            std::error_code) noexcept) noexcept;

void default_error_handler(const char *msg, std::error_code ec) noexcept;

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

} // namespace erd
