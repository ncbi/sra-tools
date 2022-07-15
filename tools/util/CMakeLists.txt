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

set ( UNIX_SRC unix/syspass.c )
set ( LINUX_SRC ${UNIX_SRC} )
set ( MAC_SRC mac/syspass.c )
set ( WIN_SRC win/syspass.c )

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
GenerateExecutableWithDefs( crc32sum "crc32sum" "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( crc32sum false )

GenerateExecutableWithDefs( test-download "test-download" "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( test-download false )

GenerateExecutableWithDefs( kdb-index "kdb-index" "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( kdb-index false )

GenerateExecutableWithDefs( pacbio-correct pacbio-correct "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( pacbio-correct false )

GenerateExecutableWithDefs( schema-replace schema-replace "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( schema-replace false )

GenerateExecutableWithDefs( sortreadtest sortreadtest "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( sortreadtest false )

GenerateExecutableWithDefs( rowwritetest rowwritetest "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( rowwritetest false )

GenerateExecutableWithDefs( samview-util samview "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( samview-util false )

GenerateExecutableWithDefs( testld testld "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( testld false )

GenerateExecutableWithDefs( txt2kdb txt2kdb "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( txt2kdb false )

GenerateExecutableWithDefs( md5cp md5cp "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( md5cp false )

# GenerateExecutableWithDefs( pseudo-aligner pseudo-aligner "" "" "${COMMON_LINK_LIBRARIES};ncbi-wvdb" )

# External tools

GenerateExecutableWithDefs( rcexplain rcexplain "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( rcexplain false )

GenerateExecutableWithDefs( kdbmeta kdbmeta "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( kdbmeta false )

GenerateExecutableWithDefs( vdb-lock vdb-lock "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( vdb-lock false )

GenerateExecutableWithDefs( vdb-unlock vdb-unlock "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( vdb-unlock false )

GenerateExecutableWithDefs( vdb-passwd "vdb-passwd;${OS_SPECIFIC_SRC}" "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
MakeLinksExe( vdb-passwd false )

if( NOT WIN32 )
GenerateExecutableWithDefs( make-read-filter make-read-filter "" "" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
MakeLinksExe( make-read-filter false )
endif()
