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

set( NGS_SRC
	NCBI-NGS
)

set( INC_DIRS
	${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs
	${CMAKE_SOURCE_DIR}
)
GenerateStaticLibsWithDefs( ncbi-ngs-c++ "${NGS_SRC}" "__mod__=\"ngs/ncbi/ngs-c++\"" "${INC_DIRS}" )
target_link_libraries( ncbi-ngs-c++ ngs )
ExportStatic( ncbi-ngs-c++ true )
if( RUN_SANITIZER_TESTS )
	target_link_libraries( ncbi-ngs-c++-asan ngs-asan )
	target_link_libraries( ncbi-ngs-c++-tsan ngs-tsan )
endif()
