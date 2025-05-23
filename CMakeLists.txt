cmake_minimum_required(VERSION 3.14)
project(fuznet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(AVAILABLE_BUILD_TYPES Debug Release)

option(ENABLE_STATIC_LINK "Link executables statically"   OFF)

if(ENABLE_STATIC_LINK)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)               # prefer/require *.a
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")
        message(STATUS "Static linking enabled for Release build")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build (Debug or Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${AVAILABLE_BUILD_TYPES})
message(STATUS "Configured for ${CMAKE_BUILD_TYPE} build")

set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include(FetchContent)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG  v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG  master
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG  v2.5.0
)
FetchContent_MakeAvailable(CLI11)

add_subdirectory(src/core)
add_subdirectory(src/netlist)
add_subdirectory(src/actions)
add_subdirectory(src/orchestrator)

add_executable(fuznet src/main.cpp)

target_link_libraries(fuznet PRIVATE orchestrator CLI11::CLI11)