if (CMAKE_SYSTEM_NAME MATCHES "Linux")
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
else ()
    message (FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif ()

if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message (FATAL_ERROR "${CMAKE_SYSTEM_PROCESSOR} is not supported")
endif ()

find_library (NCBI_VDB "ncbi-vdb" ENV NCBI_VDB_LIBDIR)
if (NCBI_VDB MATCHES NCBI_VDB-NOTFOUND)
    find_library (NCBI_VDB "ncbi-vdb" PATHS "/usr/local/ncbi" "$ENV{HOME}/usr/local/ncbi" "$ENV{HOME}/ncbi" NO_DEFAULT_PATH)
endif ()
if (NCBI_VDB MATCHES NCBI_VDB-NOTFOUND)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_library (NCBI_VDB "ncbi-vdb"
            PATHS   "$ENV{HOME}/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/lib"
                    "$ENV{HOME}/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/rel/lib"
                    "$ENV{HOME}/ncbi-outdir/ncbi-vdb/linux/clang/x86_64/dbg/lib"
                    "$ENV{HOME}/ncbi-outdir/ncbi-vdb/linux/clang/x86_64/rel/lib"
            NO_DEFAULT_PATH)
    else ()
        find_library (NCBI_VDB "ncbi-vdb"
            PATHS   "$ENV{HOME}/ncbi-outdir/ncbi-vdb/mac/clang/x86_64/dbg/lib"
                    "$ENV{HOME}/ncbi-outdir/ncbi-vdb/mac/clang/x86_64/rel/lib"
                    "$ENV{HOME}/ncbi-outdir/ncbi-vdb/mac/gcc/x86_64/dbg/lib"
                    "$ENV{HOME}/ncbi-outdir/ncbi-vdb/mac/gcc/x86_64/rel/lib"
            NO_DEFAULT_PATH)
    endif ()
endif ()

if (NCBI_VDB MATCHES NCBI_VDB-NOTFOUND)
    if (NOT ncbi-vdb_FIND_QUIETLY)
        message (WARNING "ncbi-vdb library was not found; this can be remedied by setting the environment variable NCBI_VDB_LIBDIR")
    endif ()
    if (ncbi-vdb_FIND_REQUIRED)
        message (FATAL_ERROR "required ncbi-vdb library was not found")
    endif ()
elseif (NOT ncbi-vdb_FIND_QUIETLY)
    message (STATUS "using ncbi-vdb library ${NCBI_VDB}")
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

if (NCBI_VDB MATCHES ncbi-vdb_LIBRARY-NOTFOUND)
elseif (ncbi-vdb_INCLUDE_DIR MATCHES ncbi-vdb_INCLUDE_DIR-NOTFOUND)
else ()
    set (ncbi-vdb_LIBRARIES ${NCBI_VDB})
    set (ncbi-vdb_INCLUDE_DIRS ${ncbi-vdb_INCLUDE_DIR})
    set (ncbi_vdb_FOUND 1)
endif ()
