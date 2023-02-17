<p align="center">
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="175" width="auto" />
</p>

# Energy Metrics

This project, called `erd`, is a small library to help measure energy readings.

## Building

### Pre-requisites

- CMake, version 3.14 and later
- pip

### Procedure

```shell
cmake -S . -B build
```

```shell
cmake --build build
```

Install the project:

```shell
cmake --install build --prefix <installation-directory-prefix>
```

Format the source files according to the rules defined:

```shell
cmake --build build --target fix-format
```

The Python bindings are located in `./python_bindings`. To install them:

```shell
pip install --user python_bindings
```

## Usage

The library has three components: a C++ implementation and interface, a C wrapper and Python
bindings.

### C++ Interface

```cpp
#include <erd/erd.hpp>

#include <iostream>
#include <thread>

int main() {
  constexpr uint32_t socket = 0;
  erd::attributes_t attr{erd::domain_t::package, socket};
  erd::reader_t reader{attr};
  erd::readings_t before;
  erd::readings_t after;

  if (std::error_code ec; !reader.obtain_readings(before, ec)) {
    std::cerr << "Error obtaining readings: " << ec.message() << "\n";
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  if (std::error_code ec; !reader.obtain_readings(after, ec)) {
    std::cerr << "Error obtaining readings: " << ec.message() << "\n";
    return 1;
  }

  erd::difference_t diff = reader.subtract(after, before);
  std::cout << "Duration: " << diff.duration.count() << " ns\n";
  std::cout << "Energy: " << diff.energy_consumed.count() << " uJ\n";

  return 0;
}
```

Running the above produces the output:

```txt
Found domain /sys/class/powercap/intel-rapl/intel-rapl:0 of socket 0
Duration: 1000077361 ns
Energy: 3555716 uJ
```

### C Interface

```c
#include <erd/erd.h>

#include <unistd.h>

#include <inttypes.h>
#include <stdio.h>

int main() {
  int exit_code = 0;
  char buffer[128];

  erd_status_t status = ERD_SUCCESS;
  erd_error_descriptor_t ed = {.what = buffer, .size = sizeof(buffer)};

  erd_attr_t attr = NULL;
  erd_handle_t handle = NULL;

  // pass NULL instead of &ed for no error description
  status = erd_attr_create(&attr, &ed);
  if (ERD_SUCCESS != status)
    goto error;

  status = erd_attr_set_domain(attr, ERD_PACKAGE);
  if (ERD_SUCCESS != status)
    goto error;

  status = erd_attr_set_socket(attr, 0);
  if (ERD_SUCCESS != status)
    goto error;

  // pass NULL instead of &ed for no error description
  status = erd_handle_create(&handle, attr, &ed);
  if (ERD_SUCCESS != status)
    goto error;

  erd_readings_t before;
  erd_readings_t after;
  // pass NULL instead of &ed for no error description
  status = erd_obtain_readings(handle, &before, &ed);
  if (ERD_SUCCESS != status)
    goto error;

  sleep(1);

  // pass NULL instead of &ed for no error description
  status = erd_obtain_readings(handle, &after, &ed);
  if (ERD_SUCCESS != status)
    goto error;

  erd_readings_t diff;
  status = erd_subtract_readings(handle, &after, &before, &diff);
  if (ERD_SUCCESS != status)
    goto error;

  printf("Duration: %" PRId64 " ns\n", diff.time);
  printf("Energy: %" PRIu64 " uJ\n", diff.energy);
  goto cleanup;

error:
  fprintf(stderr, "Error retrieving energy: %.*s (status code: %d)",
          (int)ed.size, ed.what, status);
  exit_code = 1;

cleanup:
  erd_attr_destroy(attr);
  erd_handle_destroy(handle);

  return exit_code;
}
```

Running the above produces the output:

```txt
Found domain /sys/class/powercap/intel-rapl/intel-rapl:0 of socket 0
Duration: 1000119139 ns
Energy: 3494498 uJ
```

### Python Interface

To work with the Python bindings, we have to set the environment variable `ERD_SHARED_LIB`
containing the path to the `*.so`:

```sh
export ERD_SHARED_LIB=/path/to/shared/lib.so
```

Otherwise, put the library in the same directory as your script and name it `liberd.so`.

```python
import time
import sys
import erd.erd as erd

try:
    socket = 0
    attr = erd.Attributes(erd.Domain.package, socket)
    handle = erd.Handle(attr)
    before: erd.Readings = handle.obtain_readings()

    time.sleep(1)

    after: erd.Readings = handle.obtain_readings()
    diff: erd.Difference = handle.subtract(after, before)

    print("Duration: {}".format(diff.duration()))
    print("Energy: {}".format(diff.energy()))
except Exception as e:
    print(e)
    sys.exit(1)
```

Running the above produces the output:

```txt
Shared library: /path/to/shared/lib.so
Found domain /sys/class/powercap/intel-rapl/intel-rapl:0 of socket 0
Duration: (1001103211, <TimeUnit.nanosecond: 1>)
Energy: (3884939, <EnergyUnit.microjoule: 1>)
```

Without `ERD_SHARED_LIB` set:

```txt
Environment variable ERD_SHARED_LIB not set
Falling back to ./liberd.so
Shared library: ./liberd.so
Found domain /sys/class/powercap/intel-rapl/intel-rapl:0 of socket 0
Duration: (1001129149, <TimeUnit.nanosecond: 1>)
Energy: (4053822, <EnergyUnit.microjoule: 1>)
```
