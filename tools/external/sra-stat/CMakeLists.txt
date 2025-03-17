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

# External
set( SRC
	assembly-statistics
	sra
	sra-stat
)
GenerateExecutableWithDefs( sra-stat "${SRC}" "__mod__=\"tools/sra-stat\"" "${CMAKE_SOURCE_DIR}/libs/inc" "" )
target_link_libraries( sra-stat kapp loader ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
if ( TARGET sra-stat-asan )
	target_link_libraries( sra-stat-asan kapp loader-asan ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
endif()
if ( TARGET sra-stat-tsan )
	target_link_libraries( sra-stat-tsan kapp loader-tsan ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
endif()
MakeLinksExe( sra-stat false )
