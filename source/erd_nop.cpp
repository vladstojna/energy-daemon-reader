#include <erd/erd_nop.hpp>

namespace erd {

reader_t::reader_t(attributes_t attr) : attr_(attr) {}

bool reader_t::obtain_readings(readings_t &into,
                               std::error_code &ec) const noexcept {
  // suppress "can be made static warning"
  (void)attr_;
  into.timestamp = clock_t::now();
  into.energy = energy_t{};
  return true;
}

difference_t reader_t::subtract(const readings_t &lhs,
                                const readings_t &rhs) const noexcept {
  (void)attr_;
  return difference_t{lhs.timestamp - rhs.timestamp, energy_t{}};
}

const attributes_t &reader_t::attributes() const noexcept { return attr_; }

} // namespace erd
