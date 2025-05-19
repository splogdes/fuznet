add_library(orchestrator STATIC
    orchestrator.hpp orchestrator.cpp
)
target_include_directories(orchestrator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(orchestrator PUBLIC netlist actions tomlplusplus::tomlplusplus)
