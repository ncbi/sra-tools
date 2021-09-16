if( WIN32 )
    set( ADDITIONAL_LIBS Crypt32 Ws2_32 )
else()
    set( ADDITIONAL_LIBS "" )
endif()

include_directories(${CMAKE_SOURCE_DIR})

set( SRC
    sqlite3.c
    sqlite3vdb.c
)
GenerateStaticLibs( vdb-sqlite "${SRC}" )

ExportStatic( vdb-sqlite false )
ExportShared( vdb-sqlite false )

add_compile_definitions (__mod__="libs/vdb-sqlite")

target_link_libraries( vdb-sqlite-shared ncbi-vdb ncbi-ngs ${ADDITIONAL_LIBS} ngs )
if( WIN32)
    target_link_libraries( vdb-sqlite-shared-md ncbi-vdb-md ncbi-ngs-md ${ADDITIONAL_LIBS} ngs )
endif()
