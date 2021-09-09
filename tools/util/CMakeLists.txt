set ( UNIX_SRC unix/PrintOS.c unix/syspass.c )
set ( LINUX_SRC ${UNIX_SRC} )
set ( MAC_SRC unix/PrintOS.c mac/syspass.c )
set ( WIN_SRC win/PrintOS.c win/syspass.c )

if ( "windows" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC ${WIN_SRC} )
    include_directories( win )
elseif( "linux" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC ${LINUX_SRC} )
    include_directories( unix )
elseif( "mac" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC ${MAC_SRC} )
    include_directories( unix )
endif()

add_compile_definitions( __mod__="tools/util" )

# include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )
# include_directories( inc )
include_directories( "." )

# Internal tools
add_executable( crc32sum crc32sum )
target_link_libraries( crc32sum ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( crc32sum )

add_executable( test-download test-download )
target_link_libraries( test-download ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( test-download )

add_executable( kdb-index kdb-index )
target_link_libraries( kdb-index ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( kdb-index )

add_executable( pacbio-correct pacbio-correct )
target_link_libraries( pacbio-correct ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( pacbio-correct )

add_executable( schema-replace schema-replace )
target_link_libraries( schema-replace ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( schema-replace )

add_executable( sortreadtest sortreadtest )
target_link_libraries( sortreadtest ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( sortreadtest )

add_executable( rowwritetest rowwritetest )
target_link_libraries( rowwritetest ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( rowwritetest )

add_executable( samview samview )
target_link_libraries( samview ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( samview )

add_executable( testld testld )
target_link_libraries( testld ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( testld )

add_executable( txt2kdb txt2kdb )
target_link_libraries( txt2kdb ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( txt2kdb )

add_executable( validate-names4 validate-names4 )
target_link_libraries( validate-names4 ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( validate-names4 )

# add_executable( md5cp md5cp )
# target_link_libraries( md5cp ${COMMON_LINK_LIBRARIES} ncbi-vdb )
# MSVS_StaticRuntime( md5cp )

# add_executable( pseudo-aligner pseudo-aligner )
# target_link_libraries( pseudo-aligner ) # ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
# MSVS_StaticRuntime( pseudo-aligner )


# External tools

add_executable( rcexplain rcexplain )
target_link_libraries( rcexplain ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( rcexplain )

add_executable( kdbmeta kdbmeta )
target_link_libraries( kdbmeta ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( kdbmeta )

add_executable( vdb-lock vdb-lock )
target_link_libraries( vdb-lock ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( vdb-lock )

add_executable( vdb-unlock vdb-unlock )
target_link_libraries( vdb-unlock ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( vdb-unlock )

add_executable( test-sra test-sra ${OS_SPECIFIC_SRC} )
target_link_libraries( test-sra ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( test-sra )

add_executable( vdb-passwd vdb-passwd ${OS_SPECIFIC_SRC} )
target_link_libraries( vdb-passwd ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( vdb-passwd )

if( NOT WIN32 )
add_executable( make-read-filter make-read-filter )
target_link_libraries( make-read-filter ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( make-read-filter )
endif()
