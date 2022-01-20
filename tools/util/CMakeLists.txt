# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

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
target_link_libraries( crc32sum ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( crc32sum false )

add_executable( test-download test-download )
target_link_libraries( test-download ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( test-download false )

add_executable( kdb-index kdb-index )
target_link_libraries( kdb-index ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( kdb-index false )

add_executable( pacbio-correct pacbio-correct )
target_link_libraries( pacbio-correct ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( pacbio-correct false )

add_executable( schema-replace schema-replace )
target_link_libraries( schema-replace ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( schema-replace false )

add_executable( sortreadtest sortreadtest )
target_link_libraries( sortreadtest ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( sortreadtest false )

add_executable( rowwritetest rowwritetest )
target_link_libraries( rowwritetest ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( rowwritetest false )

add_executable( samview-util samview )
target_link_libraries( samview-util ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( samview-util false )

add_executable( testld testld )
target_link_libraries( testld ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( testld false )

add_executable( txt2kdb txt2kdb )
target_link_libraries( txt2kdb ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( txt2kdb false )

# add_executable( md5cp md5cp )
# target_link_libraries( md5cp ${COMMON_LINK_LIBRARIES} ncbi-vdb )

# add_executable( pseudo-aligner pseudo-aligner )
# target_link_libraries( pseudo-aligner ) # ${COMMON_LINK_LIBRARIES} ncbi-wvdb )


# External tools

add_executable( rcexplain rcexplain )
target_link_libraries( rcexplain ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( rcexplain false )

add_executable( kdbmeta kdbmeta )
target_link_libraries( kdbmeta ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( kdbmeta false )

add_executable( vdb-lock vdb-lock )
target_link_libraries( vdb-lock ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( vdb-lock false )

add_executable( vdb-unlock vdb-unlock )
target_link_libraries( vdb-unlock ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( vdb-unlock false )

add_executable( test-sra test-sra ${OS_SPECIFIC_SRC} )
target_link_libraries( test-sra ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( test-sra false )

add_executable( vdb-passwd vdb-passwd ${OS_SPECIFIC_SRC} )
target_link_libraries( vdb-passwd ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-passwd false )

if( NOT WIN32 )
add_executable( make-read-filter make-read-filter )
target_link_libraries( make-read-filter ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( make-read-filter false )
endif()
