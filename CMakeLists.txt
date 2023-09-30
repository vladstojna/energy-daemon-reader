cmake_minimum_required(VERSION 3.14...3.22)

# ---- Project ----
project(
  erd
  VERSION 1.0
  LANGUAGES CXX
)

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

option(ERD_BUILD_SHARED_LIB "Build erd as a shared library for use with the Python bindings" OFF)

function(checkpowercap)
  file(GLOB powercap "/sys/class/powercap/")
  list(LENGTH powercap powercap_len)
  if(powercap_len EQUAL 0)
    set(ERD_POWERCAP
        OFF
        PARENT_SCOPE
    )
  else()
    set(ERD_POWERCAP
        ON
        PARENT_SCOPE
    )
  endif()
endfunction()

if(NOT DEFINED ERD_POWERCAP)
  checkpowercap()
endif()
if(ERD_POWERCAP)
  add_compile_definitions(ERD_POWERCAP)
else()
  message(NOTICE "[#] powercap not present, building erd without powercap support (no-op)")
endif()

if(ERD_BUILD_SHARED_LIB)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info
include(cmake/CPM.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.11.0")

CPMAddPackage(
  NAME fmt
  GIT_TAG 9.1.0
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS "FMT_INSTALL YES" # create an installable target
)

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(${PROJECT_NAME} INTERFACE)
if(ERD_BUILD_SHARED_LIB)
  add_library(${PROJECT_NAME} SHARED)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/erd/ipc/message.hpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/include/erd/units.hpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/include/erd/erd.hpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/include/erd/erd.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/erd_bindings.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/erd_common.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/message.cpp"
)
if(ERD_POWERCAP)
  target_sources(
    ${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/erd/erd_powercap.hpp"
                            "${CMAKE_CURRENT_SOURCE_DIR}/source/erd_powercap.cpp"
  )
else()
  target_sources(
    ${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/erd/erd_nop.hpp"
                            "${CMAKE_CURRENT_SOURCE_DIR}/source/erd_nop.cpp"
  )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Create an installable target ----
# this allows users to install and find the library via `find_package()`.

# the location where the project's version header will be placed should match the project's regular
# header paths
string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)

packageProject(
  NAME ${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  NAMESPACE ${PROJECT_NAME}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"
  COMPATIBILITY SameMajorVersion
  DEPENDENCIES "fmt 9.1.0"
)
