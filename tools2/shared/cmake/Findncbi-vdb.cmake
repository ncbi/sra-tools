if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set (NCBI_VDB_SYSTEM_NAME "linux")
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set (NCBI_VDB_SYSTEM_NAME "mac")
else ()
    message (FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set (NCBI_VDB_COMPILER_NAME "gcc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set (NCBI_VDB_COMPILER_NAME "clang")
else ()
    message (FATAL_ERROR "Unknown compiler ID ${CMAKE_CXX_COMPILER_ID}; compiler is not supported")
endif ()

if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message (FATAL_ERROR "${CMAKE_SYSTEM_PROCESSOR} is not supported")
endif ()

find_library (NCBI_VDB_LIB "ncbi-vdb" ENV NCBI_VDB_LIBDIR)
if (NCBI_VDB_LIB MATCHES NCBI_VDB_LIB-NOTFOUND)
    find_library (NCBI_VDB_LIB "ncbi-vdb" PATHS "/usr/local/ncbi" "$ENV{HOME}/usr/local/ncbi" "$ENV{HOME}/ncbi" NO_DEFAULT_PATH)
endif ()
if (NCBI_VDB_LIB MATCHES NCBI_VDB_LIB-NOTFOUND)
    set (NCBI_VDB_LIB_DIR_ROOT "$ENV{HOME}/ncbi-outdir/ncbi-vdb/${NCBI_VDB_SYSTEM_NAME}/${NCBI_VDB_COMPILER_NAME}/x86_64/")
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set (NCBI_VDB_LIB_DIRS "${NCBI_VDB_LIB_DIR_ROOT}/dbg/lib" "${NCBI_VDB_LIB_DIR_ROOT}/rel/lib")
    else ()
        set (NCBI_VDB_LIB_DIRS "${NCBI_VDB_LIB_DIR_ROOT}/rel/lib" "${NCBI_VDB_LIB_DIR_ROOT}/dbg/lib")
    endif ()
    find_library (NCBI_VDB_LIB "ncbi-vdb" PATHS ${NCBI_VDB_LIB_DIRS} NO_DEFAULT_PATH)
endif ()

if (NCBI_VDB_LIB MATCHES NCBI_VDB_LIB-NOTFOUND)
    if (NOT ncbi-vdb_FIND_QUIETLY)
        message ("ncbi-vdb library was not found; this can be remedied by setting the environment variable NCBI_VDB_LIBDIR")
    endif ()
    if (ncbi-vdb_FIND_REQUIRED)
        message (FATAL_ERROR "required ncbi-vdb library was not found")
    endif ()
elseif (NOT ncbi-vdb_FIND_QUIETLY)
    message (STATUS "using ncbi-vdb library ${NCBI_VDB_LIB}")
endif ()

find_path (ncbi-vdb_INCLUDE_DIR "vdb/manager.h"
    PATHS   "../ncbi-vdb/interfaces"
            "../../ncbi-vdb/interfaces"
            "../../../ncbi-vdb/interfaces"
            "../../../../ncbi-vdb/interfaces"
            "../../../../../ncbi-vdb/interfaces"
            "../../../../../../ncbi-vdb/interfaces"
            "../../../../../../../ncbi-vdb/interfaces"
)

if (ncbi-vdb_INCLUDE_DIR MATCHES ncbi-vdb_INCLUDE_DIR-NOTFOUND)
    if (NOT ncbi-vdb_FIND_QUIETLY)
        message (WARNING "ncbi-vdb headers were not found; fix it by git clone https://github.com/ncbi/ncbi-vdb.git into a parent directory")
    endif ()
    if (ncbi-vdb_FIND_REQUIRED)
        message (FATAL_ERROR "required ncbi-vdb headers were not found")
    endif ()
elseif (NOT ncbi-vdb_FIND_QUIETLY)
    message (STATUS "using ncbi-vdb headers from ${ncbi-vdb_INCLUDE_DIR}")
endif ()

if (NCBI_VDB_LIB MATCHES NCBI_VDB_LIB-NOTFOUND)
elseif (ncbi-vdb_INCLUDE_DIR MATCHES ncbi-vdb_INCLUDE_DIR-NOTFOUND)
else ()
    set (ncbi-vdb_LIBRARIES ${NCBI_VDB_LIB})
    set (ncbi-vdb_INCLUDE_DIRS ${ncbi-vdb_INCLUDE_DIR})
    set (ncbi_vdb_FOUND 1)
endif ()
