cmake_minimum_required (VERSION 3.0)

project (test)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../shared/cmake")
find_package (ncbi-vdb REQUIRED)
include_directories (${ncbi-vdb_INCLUDE_DIRS})
# link_libraries (${ncbi-vdb_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_library (PTHREAD pthread)
    link_libraries (${PTHREAD})
    find_library (MATH m)
    link_libraries (${MATH})
endif ()

include_directories (${CMAKE_SOURCE_DIR}/../../shared/include)

add_executable (test-GeneralWriter_hpp test-GeneralWriter_hpp.cpp)
target_compile_features (test-GeneralWriter_hpp PRIVATE cxx_auto_type)

enable_testing()

add_test(test-GeneralWriter_hpp test-GeneralWriter_hpp /dev/null)

