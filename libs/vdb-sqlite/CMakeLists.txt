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

include_directories(${CMAKE_SOURCE_DIR})

set( SRC
    sqlite3.c
    sqlite3vdb.c
)
GenerateStaticLibsWithDefs( vdb-sqlite "${SRC}" "__mod__=\"libs/vdb-sqlite\"" "" )

if ( CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" )
    set( CMAKE_C_FLAGS "-Wno-deprecated-declarations" )
    set_source_files_properties( sqlite3.c PROPERTIES COMPILE_FLAGS -Wno-implicit-int-float-conversion )
elseif( CMAKE_CXX_COMPILER_ID MATCHES "GNU" )
    set_source_files_properties( sqlite3.c PROPERTIES COMPILE_FLAGS -Wno-misleading-indentation )
endif()
