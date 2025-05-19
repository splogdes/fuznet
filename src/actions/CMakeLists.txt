file(GLOB ACTION_SRCS *.hpp)

add_library(actions INTERFACE)
target_sources(actions INTERFACE ${ACTION_SRCS})
target_include_directories(actions INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(actions INTERFACE netlist)