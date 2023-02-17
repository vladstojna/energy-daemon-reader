#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif // defined(__cplusplus)

typedef enum erd_status_e {
  ERD_SUCCESS,
  ERD_UNKNOWN,
  ERD_INVALID_STATUS,
  ERD_INVALID_ARGUMENT,
  ERD_INVALID_UNIT,
  ERD_SYSTEM_ERROR,
  ERD_ALLOC_ERROR,
  ERD_GENERIC_ERROR,
} erd_status_t;

typedef enum erd_energy_unit_e {
  ERD_JOULE,
  ERD_MICROJOULE,
} erd_energy_unit_t;

typedef enum erd_time_unit_e {
  ERD_SECOND,
  ERD_NANOSECOND,
} erd_time_unit_t;

typedef enum erd_rapl_domain_e {
  ERD_PACKAGE,
  ERD_UNCORE,
  ERD_CORES,
  ERD_DRAM,
} erd_rapl_domain_t;

typedef struct erd_readings_st {
  int64_t timestamp;
  uint64_t energy;
  erd_time_unit_t tunit;
  erd_energy_unit_t eunit;
} erd_readings_t;

typedef struct erd_error_descriptor_st {
  char *what = NULL;
  uint64_t size = 0;
} erd_error_descriptor_t;

typedef struct erd_attr_st *erd_attr_t;
typedef struct erd_handle_st *erd_handle_t;

erd_status_t erd_attr_create(erd_attr_t *attr, erd_error_descriptor_t *ed);

erd_status_t erd_attr_set_domain(erd_attr_t attr, erd_rapl_domain_t domain);

erd_status_t erd_attr_set_socket(erd_attr_t attr, uint32_t socket);

erd_status_t erd_attr_destroy(erd_attr_t attr);

erd_status_t erd_handle_create(erd_handle_t *handle, erd_attr_t attr,
                               erd_error_descriptor_t *ed);

erd_status_t erd_handle_destroy(erd_handle_t handle);

erd_status_t erd_obtain_readings(erd_handle_t handle, erd_readings_t *into,
                                 erd_error_descriptor_t *ed);

erd_status_t erd_subtract_readings(erd_handle_t handle,
                                   const erd_readings_t *lhs,
                                   const erd_readings_t *rhs,
                                   erd_readings_t *result);

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)
