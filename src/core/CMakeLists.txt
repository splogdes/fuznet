add_library(core STATIC
    library.hpp library.cpp
    module.hpp
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC yaml-cpp::yaml-cpp)
