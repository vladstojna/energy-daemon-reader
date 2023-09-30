#pragma once

#include <erd/erd_common.hpp>
#include <erd/units.hpp>

#include <string>
#include <system_error>

namespace erd {

class reader_t {
public:
  explicit reader_t(attributes_t attr);

  bool obtain_readings(readings_t &into, std::error_code &ec) const noexcept;

  [[nodiscard]] difference_t subtract(const readings_t &lhs,
                                      const readings_t &rhs) const noexcept;

  [[nodiscard]] const attributes_t &attributes() const noexcept;

private:
  attributes_t attr_;
};

} // namespace erd
