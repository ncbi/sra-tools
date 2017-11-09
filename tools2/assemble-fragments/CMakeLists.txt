cmake_minimum_required (VERSION 3.1)
project (assemble-fragments)

set (CMAKE_CXX_STANDARD 11)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../shared/cmake")
find_package (ncbi-vdb REQUIRED)
include_directories (${ncbi-vdb_INCLUDE_DIRS})
link_libraries (${ncbi-vdb_LIBRARIES})

include_directories (${CMAKE_SOURCE_DIR}/../shared/include)

add_executable (sra2ir sra2ir.cpp)
add_executable (sam2ir sam2ir.cpp)
add_executable (reorder-ir reorder-ir.cpp)
add_executable (filter-ir filter-ir.cpp)
add_executable (summarize-pairs summarize-pairs.cpp)
add_executable (assemble-fragments assemble-fragments.cpp)
