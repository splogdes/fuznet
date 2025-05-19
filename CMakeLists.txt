cmake_minimum_required(VERSION 3.20)
project(fuznet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
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