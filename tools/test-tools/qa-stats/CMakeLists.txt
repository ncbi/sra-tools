cmake_minimum_required (VERSION 3.8)

project(qa-stats)

set(SOURCES 
    main.cpp
    input.cpp
    output.cpp
)

set(HEADERS
    input.hpp
    hashing.hpp
    output.hpp
    parameters.hpp
)

find_package(Threads REQUIRED)

add_executable(qa-stats ${SOURCES} ${HEADERS})
target_compile_features(qa-stats PRIVATE cxx_std_17)
target_link_libraries(qa-stats Threads::Threads)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
