
include_directories(${CMAKE_SOURCE_DIR})

set( SRC
    sqlite3.c
    sqlite3vdb.c
)
GenerateStaticLibs( vdb-sqlite "${SRC}" )

add_compile_definitions (__mod__="libs/vdb-sqlite")
