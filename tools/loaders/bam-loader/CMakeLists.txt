#===========================================================================
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
if( NOT WIN32)

	add_compile_definitions( __mod__="tools/bam-loader" )

    include_directories( ${VDB_INTERFACES_DIR}/ext/ ) # zlib.h
	include_directories(./)

    set( SRC
        bam-loader
        bam
        sam
        alignment-writer
        reference-writer
        sequence-writer
        loader-imp
        mem-bank
        low-match-count
    )
    set_source_files_properties(bam-loader.c PROPERTIES LANGUAGE CXX )
    set_source_files_properties(loader-imp.c PROPERTIES LANGUAGE CXX )

    GenerateExecutableWithDefs( bam-load "${SRC}" "" "${CMAKE_SOURCE_DIR}/libs/inc" "kapp;loader;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
    target_compile_features(bam-load PRIVATE cxx_std_17)
    if (COMPILER_OPTION_SSE42_SUPPORTED)
        target_compile_options( bam-load PRIVATE -msse4.2 -DBMSSE42OPT)
    endif()
    target_include_directories( bam-load PRIVATE ${CMAKE_SOURCE_DIR}/libs/inc)
    target_link_libraries( bam-load  loader ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE})

    MakeLinksExe( bam-load false )

	# Internal
	GenerateExecutableWithDefs( samview "bam;sam;samview" "" "" "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
	MakeLinksExe( samview false )

endif()
