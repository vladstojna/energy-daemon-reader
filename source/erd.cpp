#include <erd/erd.hpp>
#include <fmt/format.h>

#include <charconv>
#include <fstream>
#include <iostream>
#include <set>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr char DOMAIN_PKG_PREFIX[] = "package";
constexpr char DOMAIN_PP0[] = "core";
constexpr char DOMAIN_PP1[] = "uncore";
constexpr char DOMAIN_DRAM[] = "dram";

auto default_error_handler_v =
    +[](const char *msg, std::error_code ec) noexcept {
      std::cerr << msg << " (" << ec << ")" << std::endl;
    };

auto default_output_v =
    +[](const char *msg) noexcept { std::cout << msg << "\n"; };

std::error_code get_errno() noexcept {
  return std::error_code{errno, std::system_category()};
}

ssize_t read_buff(int fd, char *buffer, size_t buffsz) noexcept {
  ssize_t ret;
  ret = pread(fd, buffer, buffsz - 1, 0);
  if (ret > 0)
    buffer[ret] = '\0';
  return ret;
}

bool read_uint64(const erd::detail::file_descriptor &fd, uint64_t &into,
                 std::error_code &ec) noexcept {
  constexpr size_t MAX_UINT64_SZ = 24;
  char buffer[MAX_UINT64_SZ];
  ssize_t bytes_read = read_buff(int(fd), buffer, MAX_UINT64_SZ);
  if (bytes_read <= 0) {
    ec = get_errno();
    return false;
  }
  auto [ptr, errcode] = std::from_chars(buffer, buffer + bytes_read, into);
  if (ec = std::make_error_code(errcode); ec) {
    return false;
  }
  ec.clear();
  return true;
}

bool file_exists(std::string_view path) noexcept {
  return !access(path.data(), F_OK);
}

bool count_sockets(uint32_t &into, std::error_code &ec) {
  char filename[128];
  std::set<uint32_t> packages;
  for (int i = 0;; i++) {
    snprintf(filename, sizeof(filename),
             "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
    if (!file_exists(filename))
      break;
    uint64_t pkg;
    if (!read_uint64(erd::detail::file_descriptor{filename}, pkg, ec)) {
      return false;
    }
    packages.insert(static_cast<uint32_t>(pkg));
  };
  ec.clear();
  into = static_cast<uint32_t>(packages.size());
  return true;
}

bool is_package_domain(const char *name) noexcept {
  return !std::strncmp(DOMAIN_PKG_PREFIX, name, sizeof(DOMAIN_PKG_PREFIX) - 1);
}

bool domain_from_name(const char *name, erd::domain_t &into,
                      std::error_code &ec) noexcept {
  ec.clear();
  if (is_package_domain(name)) {
    into = erd::domain_t::package;
  } else if (!std::strncmp(DOMAIN_PP0, name, sizeof(DOMAIN_PP0) - 1)) {
    into = erd::domain_t::cores;
  } else if (!std::strncmp(DOMAIN_PP1, name, sizeof(DOMAIN_PP1) - 1)) {
    into = erd::domain_t::uncore;
  } else if (!std::strncmp(DOMAIN_DRAM, name, sizeof(DOMAIN_DRAM) - 1)) {
    into = erd::domain_t::dram;
  } else {
    ec = std::make_error_code(std::errc::invalid_argument);
  }
  if (ec) {
    erd::default_error_handler("No domain name matches available matches", ec);
    return false;
  }
  return true;
}

bool domain_from_path(const char *prefix, erd::domain_t &into,
                      std::error_code &ec) {
  // open the <prefix>/name file, read the name and obtain the domain
  char name[32];
  char filename[128];
  snprintf(filename, sizeof(filename), "%s/name", prefix);
  erd::detail::file_descriptor fd{filename};

  if (read_buff(int(fd), name, sizeof(name)) < 0) {
    ec = get_errno();
    return false;
  }
  if (!domain_from_name(name, into, ec)) {
    return false;
  }
  ec.clear();
  return true;
}

bool get_package_number(const char *base, uint32_t &into, std::error_code &ec) {
  char name[32];
  char filename[128];
  // read the <domain>/name content
  snprintf(filename, sizeof(filename), "%s/name", base);
  erd::detail::file_descriptor fd{filename};
  ssize_t namelen = read_buff(int(fd), name, sizeof(name));
  if (namelen < 0) {
    ec = get_errno();
    return false;
  }
  // check whether the contents follow the package-<number> pattern
  if (!is_package_domain(name)) {
    ec = std::make_error_code(std::errc::invalid_argument);
    erd::default_error_handler(
        fmt::format("Domain {} does not point to a package domain", base)
            .c_str(),
        ec);
    return false;
  }
  // offset package- to point to the package number;
  // null-terminator counts as the dash
  const char *pkg_num_start = name + sizeof(DOMAIN_PKG_PREFIX);
  auto [p, err] = std::from_chars(pkg_num_start, name + namelen, into, 10);
  if (ec = std::make_error_code(err); ec) {
    return false;
  }
  ec.clear();
  return true;
}

bool get_sensor_file_prefix(const erd::attributes_t &attr, char *path,
                            size_t sz, std::error_code &ec) {
  uint32_t num_sockets;
  if (!count_sockets(num_sockets, ec)) {
    return false;
  }
  if (attr.socket >= num_sockets) {
    ec = std::make_error_code(std::errc::invalid_argument);
    erd::default_error_handler(
        fmt::format("System has {} sockets but socket {} in argument",
                    num_sockets, attr.socket)
            .c_str(),
        ec);
    return false;
  }
  for (uint32_t skt = 0; skt < num_sockets; skt++) {
    int written =
        snprintf(path, sz, "/sys/class/powercap/intel-rapl/intel-rapl:%u", skt);
    if (file_exists(path)) {
      uint32_t pkg_number;
      if (!get_package_number(path, pkg_number, ec)) {
        return false;
      }
      if (pkg_number == attr.socket) {
        if (attr.domain == erd::domain_t::package) {
          erd::default_output(
              fmt::format("Found domain {} of socket {}", path, pkg_number)
                  .c_str());
          return true;
        }
        for (uint32_t domain_count = 0;
             domain_count < static_cast<uint32_t>(erd::domain_t::dram);
             ++domain_count) {
          snprintf(path + written, sz - written, "/intel-rapl:%u:%u", skt,
                   domain_count);
          if (file_exists(path)) {
            if (erd::domain_t dmn; !domain_from_path(path, dmn, ec)) {
              return false;
            } else if (dmn == attr.domain) {
              erd::default_output(
                  fmt::format("Found domain {} of socket {}", path, pkg_number)
                      .c_str());
              return true;
            }
          }
        }
        ec = std::make_error_code(std::errc::invalid_argument);
        erd::default_error_handler(
            fmt::format("No valid domain was found for socket {}", pkg_number)
                .c_str(),
            ec);
        return false;
      }
    }
  }
  ec = std::make_error_code(std::errc::invalid_argument);
  erd::default_error_handler(
      fmt::format("No matching socket found for {}", attr.socket).c_str(), ec);
  return false;
}

std::string get_sensor_file_prefix(const erd::attributes_t &attr) {
  char path[128];
  if (std::error_code ec;
      !get_sensor_file_prefix(attr, path, sizeof(path), ec)) {
    throw std::system_error(ec);
  }
  return path;
}

uint64_t get_sensor_max_value(const std::string &prefix) {
  erd::detail::file_descriptor fd{prefix + "/max_energy_range_uj"};
  uint64_t value;
  if (std::error_code ec; !read_uint64(fd, value, ec)) {
    throw std::system_error(ec);
  }
  return value;
}

erd::detail::file_descriptor get_sensor_fd(const std::string &prefix) {
  return erd::detail::file_descriptor{prefix + "/energy_uj"};
}

} // namespace

namespace erd::detail {

file_descriptor::file_descriptor(const std::string &path)
    : file_descriptor(path.c_str()) {}

file_descriptor::file_descriptor(const char *file)
    : value_(open(file, O_RDONLY)) {
  if (value_ == -1)
    throw std::system_error(get_errno());
}

file_descriptor::file_descriptor(const file_descriptor &other)
    : value_(dup(other.value_)) {
  if (value_ == -1)
    throw std::system_error(get_errno());
}

file_descriptor::file_descriptor(file_descriptor &&other) noexcept
    : value_(std::exchange(other.value_, -1)) {}

file_descriptor &file_descriptor::operator=(const file_descriptor &other) {
  *this = file_descriptor{other};
  return *this;
}

file_descriptor::~file_descriptor() noexcept {
  if (value_ >= 0 && close(value_) == -1)
    perror("file_descriptor: error closing file");
}

file_descriptor &file_descriptor::operator=(file_descriptor &&other) noexcept {
  value_ = std::exchange(other.value_, -1);
  return *this;
}

file_descriptor::operator int() const noexcept { return value_; }

} // namespace erd::detail

namespace erd {

reader_t::reader_t(attributes_t attr)
    : reader_t(std::move(attr), get_sensor_file_prefix(attr)) {}

reader_t::reader_t(attributes_t &&attr, const std::string &prefix)
    : attr_(std::move(attr)), sensor_(get_sensor_fd(prefix)),
      maxvalue_(get_sensor_max_value(prefix)) {}

bool reader_t::obtain_readings(readings_t &into,
                               std::error_code &ec) const noexcept {
  if (uint64_t energy_value; read_uint64(sensor_, energy_value, ec)) {
    into.timestamp = clock_t::now();
    into.energy = energy_t{energy_value};
    return true;
  }
  return false;
}

difference_t reader_t::subtract(const readings_t &lhs,
                                const readings_t &rhs) const noexcept {

  // potential overflow occured
  if (rhs.energy > lhs.energy) {
    energy_t new_energy = maxvalue_ + lhs.energy;
    return difference_t{lhs.timestamp - rhs.timestamp, new_energy - rhs.energy};
  }
  return difference_t{lhs.timestamp - rhs.timestamp, lhs.energy - rhs.energy};
}

const attributes_t &reader_t::attributes() const noexcept { return attr_; }

void set_default_output(void (*func)(const char *) noexcept) noexcept {
  default_output_v = func;
}

void default_output(const char *msg) noexcept { default_output_v(msg); }

void set_default_error_handler(
    void (*func)(const char *, std::error_code) noexcept) noexcept {
  default_error_handler_v = func;
}

void default_error_handler(const char *msg, std::error_code ec) noexcept {
  return default_error_handler_v(msg, std::move(ec));
}

} // namespace erd
