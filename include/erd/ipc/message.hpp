#pragma once

#include <erd/erd.hpp>

#include <cstdint>

namespace erd::ipc {

enum class operation_type_t : uint32_t {
  obtain_readings,
  subtract,
};

enum class status_code_t : uint32_t {
  success,
  error,
};

enum class unit_energy_t : uint16_t {
  joule,
  microjoule,
};

enum class unit_time_t : uint16_t {
  second,
  nanosecond,
};

namespace detail {

struct message_common {
public:
  static constexpr size_t size = sizeof(operation_type_t);

  [[nodiscard]] operation_type_t operation_type() const noexcept;

  char *buffer() noexcept;

  [[nodiscard]] const char *buffer() const noexcept;

protected:
  void serialize(operation_type_t optype) noexcept;

private:
  char buffer_[size];
};

} // namespace detail

class message_request : public detail::message_common {
public:
  static constexpr size_t size = detail::message_common::size + 40;

  bool readings(readings_t &lhs, readings_t &rhs,
                std::error_code &ec) const noexcept;

  void serialize() noexcept;
  void serialize(const readings_t &lhs, const readings_t &rhs) noexcept;

private:
  char buffer_[size - detail::message_common::size];
};

class message_response : public detail::message_common {
public:
  static constexpr size_t size = detail::message_common::size + 24;

  [[nodiscard]] status_code_t status_code() const noexcept;

  bool readings(readings_t &into, std::error_code &ec) const noexcept;

  bool difference(difference_t &into, std::error_code &ec) const noexcept;

  void serialize(status_code_t status, const difference_t &data) noexcept;
  void serialize(status_code_t status, const readings_t &data) noexcept;

private:
  char buffer_[size - detail::message_common::size];
};

} // namespace erd::ipc
