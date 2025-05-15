cmake_minimum_required(VERSION 3.14)
project(fuznet CXX)

# Optional: force C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include yaml-cpp from external
add_subdirectory(external/yaml-cpp)

# Add your own source files
file(GLOB_RECURSE FUZNET_SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Declare your executable
add_executable(fuznet ${FUZNET_SOURCES})

# Link yaml-cpp
target_link_libraries(fuznet PRIVATE yaml-cpp)

# Include your headers if needed
target_include_directories(fuznet PRIVATE src)
