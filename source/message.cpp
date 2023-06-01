#include <erd/ipc/message.hpp>

#include <cstdlib>
#include <cstring>

namespace {

template <typename T> T retrieve_field(const char *from) {
  T val;
  std::memcpy(&val, from, sizeof(val));
  return val;
}

template <typename T> T retrieve_field_advance(const char *&from) {
  T val = ::retrieve_field<T>(from);
  from += sizeof(val);
  return val;
}

template <typename T> void insert_field(char *to, const T &val) {
  std::memcpy(to, &val, sizeof(T));
}

template <typename T> void insert_field_advance(char *&to, const T &val) {
  ::insert_field(to, val);
  to += sizeof(T);
}

template <typename T>
bool units_to_generic(T &into, erd::ipc::unit_time_t tunit, int64_t time) {
  using erd::ipc::unit_time_t;
  switch (tunit) {
  case unit_time_t::second:
    into = T{std::chrono::seconds{time}};
    return true;
  case unit_time_t::nanosecond:
    into = T{erd::clock_t::duration{time}};
    return true;
  }
  return false;
}

bool units_to_timepoint(erd::time_point_t &into, erd::ipc::unit_time_t tunit,
                        int64_t time) {
  return units_to_generic(into, tunit, time);
}

bool units_to_duration(erd::clock_t::duration &into,
                       erd::ipc::unit_time_t tunit, int64_t time) {
  return units_to_generic(into, tunit, time);
}

bool units_to_energy(erd::microjoules<uint64_t> &into,
                     erd::ipc::unit_energy_t eunit, uint64_t energy) {
  using erd::ipc::unit_energy_t;
  switch (eunit) {
  case unit_energy_t::joule:
    into = erd::joules<uint64_t>(energy);
    return true;
  case unit_energy_t::microjoule:
    into = erd::microjoules<uint64_t>(energy);
    return true;
  }
  return true;
}

bool deserialize_readings(const char *position, erd::readings_t &into,
                          std::error_code &ec) noexcept {
  auto time = ::retrieve_field_advance<int64_t>(position);
  auto energy = ::retrieve_field_advance<uint64_t>(position);
  auto tunit = ::retrieve_field_advance<erd::ipc::unit_time_t>(position);
  auto eunit = ::retrieve_field_advance<erd::ipc::unit_energy_t>(position);

  if (!::units_to_timepoint(into.timestamp, tunit, time)) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  if (!::units_to_energy(into.energy, eunit, energy)) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  ec.clear();
  return true;
}

size_t serialize_readings(char *position, const erd::readings_t &data) {
  const char *start = position;
  ::insert_field_advance(position, data.timestamp.time_since_epoch().count());
  ::insert_field_advance(position, data.energy.count());
  ::insert_field_advance(position, erd::ipc::unit_time_t::nanosecond);
  ::insert_field_advance(position, erd::ipc::unit_energy_t::microjoule);
  return position - start;
}

} // namespace

namespace erd::ipc {

namespace detail {

operation_type_t message_common::operation_type() const noexcept {
  return ::retrieve_field<operation_type_t>(buffer_);
}

char *message_common::buffer() noexcept { return buffer_; }

const char *message_common::buffer() const noexcept { return buffer_; }

void message_common::serialize(operation_type_t optype) noexcept {
  ::insert_field(buffer_, optype);
}

} // namespace detail

status_code_t erd::ipc::message_response::status_code() const noexcept {
  return ::retrieve_field<status_code_t>(buffer_);
}

bool message_response::readings(erd::readings_t &into,
                                std::error_code &ec) const noexcept {
  if (operation_type() != erd::ipc::operation_type_t::obtain_readings) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  return ::deserialize_readings(buffer_ + sizeof(status_code_t), into, ec);
}

bool message_response::difference(erd::difference_t &into,
                                  std::error_code &ec) const noexcept {
  if (operation_type() != operation_type_t::subtract) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }

  const char *position = buffer_ + sizeof(status_code_t);
  auto time = ::retrieve_field_advance<int64_t>(position);
  auto energy = ::retrieve_field_advance<uint64_t>(position);
  auto tunit = ::retrieve_field_advance<unit_time_t>(position);
  auto eunit = ::retrieve_field_advance<unit_energy_t>(position);

  if (!::units_to_duration(into.duration, tunit, time)) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  if (!::units_to_energy(into.energy_consumed, eunit, energy)) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  ec.clear();
  return true;
}

void message_response::serialize(status_code_t status,
                                 const difference_t &data) noexcept {
  detail::message_common::serialize(operation_type_t::subtract);
  char *position = buffer_ + sizeof(status_code_t);
  ::insert_field(buffer_, status);
  ::insert_field_advance(position, data.duration.count());
  ::insert_field_advance(position, data.energy_consumed.count());
  ::insert_field_advance(position, erd::ipc::unit_time_t::nanosecond);
  ::insert_field_advance(position, erd::ipc::unit_energy_t::microjoule);
}

void message_response::serialize(status_code_t status,
                                 const readings_t &data) noexcept {
  detail::message_common::serialize(operation_type_t::obtain_readings);
  ::insert_field(buffer_, status);
  ::serialize_readings(buffer_ + sizeof(status), data);
}

bool message_request::readings(readings_t &lhs, readings_t &rhs,
                               std::error_code &ec) const noexcept {
  if (operation_type() != erd::ipc::operation_type_t::subtract) {
    ec = std::make_error_code(std::errc::bad_message);
    return false;
  }
  if (!::deserialize_readings(buffer_, lhs, ec)) {
    return false;
  }
  constexpr uint32_t READINGS_BYTE_COUNT = 20;
  return ::deserialize_readings(buffer_ + READINGS_BYTE_COUNT, rhs, ec);
}

void message_request::serialize() noexcept {
  detail::message_common::serialize(operation_type_t::obtain_readings);
}

void message_request::serialize(const readings_t &lhs,
                                const readings_t &rhs) noexcept {
  detail::message_common::serialize(operation_type_t::subtract);
  char *position = buffer_;
  position += ::serialize_readings(position, lhs);
  ::serialize_readings(position, rhs);
}

} // namespace erd::ipc
