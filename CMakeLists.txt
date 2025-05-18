cmake_minimum_required(VERSION 3.14)
project(fuznet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(external/yaml-cpp)

file(GLOB_RECURSE FUZNET_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(fuznet ${FUZNET_SOURCES})

target_link_libraries(fuznet PRIVATE yaml-cpp)

target_include_directories(fuznet PRIVATE src)
