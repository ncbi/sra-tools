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

cmake_minimum_required (VERSION 3.8)

project(rnd2sra)

if (WIN32)
	set( RND2SRALIBS Ws2_32 Shell32 Crypt32 Advapi32 Ole32 ${COMMON_LIBS_WRITE} )
else()
	set( RND2SRALIBS "${COMMON_LIBS_WRITE}" )
endif()

if (LINUX)
	list( APPEND RND2SRALIBS "stdc++fs" )
endif()

GenerateExecutableWithDefs( rnd2sra
    src/rnd2sra.cpp
    ""
    ""
    "${RND2SRALIBS}" )

target_compile_features( rnd2sra PRIVATE cxx_std_17 )

if (WIN32)
	set_target_properties( rnd2sra PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup" )
endif()

GenerateExecutableWithDefs( test_rnd2sra
    src/test_rnd2sra.cpp
    ""
    ""
    "${RND2SRALIBS}" )

target_compile_features( test_rnd2sra PRIVATE cxx_std_17 )

if (WIN32)
	set_target_properties( test_rnd2sra PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup" )
endif()
