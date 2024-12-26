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

add_compile_definitions( __mod__="tools/internal/vdb-sql" )

# External
set( VDB_SQL_LIB
	vdb-sqlite
	ngs
	${COMMON_LIBS_READ}
)
if( WIN32 )
	list( APPEND VDB_SQL_LIB
        libcmt$<$<CONFIG:Debug>:d>
        libcpmt$<$<CONFIG:Debug>:d>
        libucrt$<$<CONFIG:Debug>:d>
        libvcruntime$<$<CONFIG:Debug>:d>
    )
endif()

GenerateExecutableWithDefs( vdb-sql "vdb_shell" "" "" "kapp;${VDB_SQL_LIB};${COMMON_LINK_LIBRARIES}" )
MakeLinksExe( vdb-sql false )

GenerateExecutableWithDefs( fastconv "fastconv" "" "" "kapp;${COMMON_LINK_LIBRARIES};${VDB_SQL_LIB}" )
MakeLinksExe( fastconv false )
