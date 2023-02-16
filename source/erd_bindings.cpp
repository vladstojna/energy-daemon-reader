#include <erd/erd.h>
#include <erd/erd.hpp>

#include <cassert>
#include <cstring>
#include <string_view>

namespace {

void fill_error_descriptor(erd_error_descriptor_t *err,
                           std::string_view content) noexcept {
  if (!err || !err->size || !content.size())
    return;
  auto min = std::min(err->size - 1, content.size());
  assert(min);
  std::strncpy(err->what, content.data(), min);
  err->what[min] = 0;
  err->size = min;
}

erd_status_t unknown_error(erd_error_descriptor_t *ed) noexcept {
  fill_error_descriptor(ed, "unknown");
  return ERD_UNKNOWN;
}

erd_status_t system_error(erd_error_descriptor_t *ed,
                          std::string_view msg) noexcept {
  fill_error_descriptor(ed, msg);
  return ERD_SYSTEM_ERROR;
}

erd_status_t alloc_error(erd_error_descriptor_t *ed,
                         std::string_view msg) noexcept {
  fill_error_descriptor(ed, msg);
  return ERD_ALLOC_ERROR;
}

erd_status_t generic_error(erd_error_descriptor_t *ed,
                           std::string_view msg) noexcept {
  fill_error_descriptor(ed, msg);
  return ERD_GENERIC_ERROR;
}

erd_status_t default_exception_handler(erd_error_descriptor_t *ed) noexcept {
  try {
    throw;
  } catch (const std::system_error &e) {
    return system_error(ed, e.what());
  } catch (const std::bad_alloc &e) {
    return system_error(ed, e.what());
  } catch (const std::exception &e) {
    return generic_error(ed, e.what());
  } catch (...) {
    return unknown_error(ed);
  }
}

} // namespace

erd_status_t erd_attr_create(erd_attr_t *attr, erd_error_descriptor_t *ed) {
  try {
    *attr = reinterpret_cast<erd_attr_t>(new erd::attributes_t{});
  } catch (...) {
    return alloc_error(ed, "Error allocating memory for attributes");
  }
  return ERD_SUCCESS;
}

erd_status_t erd_attr_set_domain(erd_attr_t attr, erd_rapl_domain_t domain) {
  erd::attributes_t &a = *reinterpret_cast<erd::attributes_t *>(attr);
  switch (domain) {
  case ERD_PACKAGE:
    a.domain = erd::domain_t::package;
    return ERD_SUCCESS;
  case ERD_CORES:
    a.domain = erd::domain_t::cores;
    return ERD_SUCCESS;
  case ERD_UNCORE:
    a.domain = erd::domain_t::uncore;
    return ERD_SUCCESS;
  case ERD_DRAM:
    a.domain = erd::domain_t::dram;
    return ERD_SUCCESS;
  }
  return ERD_INVALID_ARGUMENT;
}

erd_status_t erd_attr_set_socket(erd_attr_t attr, uint32_t socket) {
  erd::attributes_t &a = *reinterpret_cast<erd::attributes_t *>(attr);
  a.socket = socket;
  return ERD_SUCCESS;
}

erd_status_t erd_attr_destroy(erd_attr_t attr) {
  if (attr)
    delete reinterpret_cast<erd::attributes_t *>(attr);
  return ERD_SUCCESS;
}

erd_status_t erd_handle_create(erd_handle_t *handle, erd_attr_t attr,
                               erd_error_descriptor_t *ed) {
  try {
    const erd::attributes_t &a = *reinterpret_cast<erd::attributes_t *>(attr);
    erd::reader_t *reader = new erd::reader_t{a};
    *handle = reinterpret_cast<erd_handle_t>(reader);
  } catch (...) {
    return default_exception_handler(ed);
  }
  return ERD_SUCCESS;
}

erd_status_t erd_handle_destroy(erd_handle_t handle) {
  if (handle)
    delete reinterpret_cast<erd::reader_t *>(handle);
  return ERD_SUCCESS;
}

erd_status_t erd_obtain_readings(erd_handle_t handle, erd_readings_t *into,
                                 erd_error_descriptor_t *ed) {
  try {
    const erd::reader_t &reader = *reinterpret_cast<erd::reader_t *>(handle);
    erd::readings_t readings;
    if (std::error_code ec; !reader.obtain_readings(readings, ec)) {
      erd::default_error_handler("Error obtaining readings", ec);
      if (ec.category() == std::system_category()) {
        return system_error(ed, ec.message());
      }
      return generic_error(ed, ec.message());
    }
    into->timestamp = readings.timestamp.time_since_epoch().count();
    into->energy = readings.energy.count();
    into->tunit = ERD_NANOSECOND;
    into->eunit = ERD_MICROJOULE;
  } catch (...) {
    return default_exception_handler(ed);
  }
  return ERD_SUCCESS;
}

erd_status_t erd_subtract_readings(erd_handle_t handle,
                                   const erd_readings_t *lhs,
                                   const erd_readings_t *rhs,
                                   erd_readings_t *result) {
  try {
    const erd::reader_t &reader = *reinterpret_cast<erd::reader_t *>(handle);
    erd::readings_t left{
        erd::time_point_t{erd::clock_t::duration{lhs->timestamp}},
        erd::energy_t{lhs->energy}};
    erd::readings_t right{
        erd::time_point_t{erd::clock_t::duration{rhs->timestamp}},
        erd::energy_t{rhs->energy}};
    erd::difference_t diff = reader.subtract(left, right);
    result->timestamp = diff.duration.count();
    result->energy = diff.energy_consumed.count();
    result->tunit = ERD_NANOSECOND;
    result->eunit = ERD_MICROJOULE;
  } catch (...) {
    return ERD_UNKNOWN;
  }
  return ERD_SUCCESS;
}
