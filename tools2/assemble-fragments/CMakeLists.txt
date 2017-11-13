cmake_minimum_required (VERSION 2.8)
project (assemble-fragments)

set (CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../shared/cmake")
find_package (ncbi-vdb REQUIRED)
include_directories (${ncbi-vdb_INCLUDE_DIRS})
link_libraries (${ncbi-vdb_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_library (PTHREAD pthread)
    link_libraries (${PTHREAD})
endif ()

include_directories (${CMAKE_SOURCE_DIR}/../shared/include)

add_executable (sra2ir sra2ir.cpp)
add_executable (sam2ir sam2ir.cpp)
add_executable (reorder-ir reorder-ir.cpp)
add_executable (filter-ir filter-ir.cpp)
add_executable (summarize-pairs summarize-pairs.cpp)
add_executable (assemble-fragments assemble-fragments.cpp)
