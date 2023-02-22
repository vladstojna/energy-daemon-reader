cmake_minimum_required(VERSION 3.14...3.22)

project(Client LANGUAGES CXX)

# --- Import tools ----
include(../cmake/tools.cmake)

# ---- Dependencies ----
include(../cmake/CPM.cmake)

CPMAddPackage(
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 3.0.0
  OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
)

set(ASIO_REPOSITORY
    "https://github.com/chriskohlhoff/asio"
    CACHE STRING "Repository of asio"
)
set(ASIO_TAG
    "asio-1-24-0"
    CACHE STRING "Git tag of asio"
)

CPMAddPackage(
  NAME asiocmake
  GITHUB_REPOSITORY OlivierLDff/asio.cmake
  GIT_TAG "main"
  OPTIONS "ASIO_USE_CPM ON"
)

CPMAddPackage(NAME EnergyReadingDaemon SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

# ---- Create standalone executable ----
file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 OUTPUT_NAME "Client")

target_link_libraries(${PROJECT_NAME} EnergyReadingDaemon::EnergyReadingDaemon cxxopts)
target_link_libraries(${PROJECT_NAME} EnergyReadingDaemon::EnergyReadingDaemon asio::asio)
