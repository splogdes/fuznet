add_library(reducer STATIC
    reducer.hpp reducer.cpp
)
target_include_directories(reducer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(reducer PUBLIC
                      netlist)
