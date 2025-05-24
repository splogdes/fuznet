cmake_minimum_required(VERSION 3.14)
project(fuznet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(AVAILABLE_BUILD_TYPES Debug Release)
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Debug or Release" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${AVAILABLE_BUILD_TYPES})
message(STATUS "Configured for ${CMAKE_BUILD_TYPE} build")

set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")


option(ENABLE_STATIC_LINK "Link executables statically" OFF)
if(ENABLE_STATIC_LINK AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")
  message(STATUS "Static linking enabled")
endif()

include(FetchContent)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG        v3.4.0
)
FetchContent_Declare(
  yaml_cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        master
)
FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.5.0
)


FetchContent_MakeAvailable(tomlplusplus yaml_cpp CLI11)

add_subdirectory(src/core)
add_subdirectory(src/netlist)
add_subdirectory(src/actions)
add_subdirectory(src/orchestrator)

add_executable(fuznet src/main.cpp)

target_link_libraries(fuznet
  PRIVATE
    orchestrator
    CLI11::CLI11
)


install(TARGETS fuznet
        RUNTIME DESTINATION bin)