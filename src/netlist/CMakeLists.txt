add_library(netlist STATIC
    netlist.hpp netlist.cpp
)
target_include_directories(netlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(netlist PUBLIC
                      core
                      nlohmann_json::nlohmann_json)
