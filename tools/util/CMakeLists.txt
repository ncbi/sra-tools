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
target_link_libraries( crc32sum ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( crc32sum true )

add_executable( test-download test-download )
target_link_libraries( test-download ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( test-download true )

add_executable( kdb-index kdb-index )
target_link_libraries( kdb-index ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( kdb-index true )

add_executable( pacbio-correct pacbio-correct )
target_link_libraries( pacbio-correct ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( pacbio-correct true )

add_executable( schema-replace schema-replace )
target_link_libraries( schema-replace ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( schema-replace true )

add_executable( sortreadtest sortreadtest )
target_link_libraries( sortreadtest ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( sortreadtest true )

add_executable( rowwritetest rowwritetest )
target_link_libraries( rowwritetest ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( rowwritetest true )

add_executable( samview-util samview )
target_link_libraries( samview-util ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( samview-util true )

add_executable( testld testld )
target_link_libraries( testld ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( testld true )

add_executable( txt2kdb txt2kdb )
target_link_libraries( txt2kdb ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( txt2kdb true )

add_executable( validate-names4 validate-names4 )
target_link_libraries( validate-names4 ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( validate-names4 true )

# add_executable( md5cp md5cp )
# target_link_libraries( md5cp ${COMMON_LINK_LIBRARIES} ncbi-vdb )

# add_executable( pseudo-aligner pseudo-aligner )
# target_link_libraries( pseudo-aligner ) # ${COMMON_LINK_LIBRARIES} ncbi-wvdb )


# External tools

add_executable( rcexplain rcexplain )
target_link_libraries( rcexplain ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( rcexplain true )

add_executable( kdbmeta kdbmeta )
target_link_libraries( kdbmeta ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( kdbmeta true )

add_executable( vdb-lock vdb-lock )
target_link_libraries( vdb-lock ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( vdb-lock true )

add_executable( vdb-unlock vdb-unlock )
target_link_libraries( vdb-unlock ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( vdb-unlock true )

add_executable( test-sra test-sra ${OS_SPECIFIC_SRC} )
target_link_libraries( test-sra ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( test-sra true )

add_executable( vdb-passwd vdb-passwd ${OS_SPECIFIC_SRC} )
target_link_libraries( vdb-passwd ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( vdb-passwd true )

if( NOT WIN32 )
add_executable( make-read-filter make-read-filter )
target_link_libraries( make-read-filter ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
MakeLinksExe( make-read-filter true )
endif()
