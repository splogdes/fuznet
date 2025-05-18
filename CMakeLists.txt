cmake_minimum_required(VERSION 3.20)
project(fuznet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(external/yaml-cpp)
add_subdirectory(external/tomlplusplus)

add_subdirectory(src/core)
add_subdirectory(src/netlist)
add_subdirectory(src/actions)

add_executable(fuznet src/main.cpp)

target_link_libraries(fuznet PRIVATE yaml-cpp netlist)