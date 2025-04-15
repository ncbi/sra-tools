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

add_compile_definitions( __mod__="tools/fastq-loader" )

if( FLEX_FOUND AND BISON_FOUND )
    FLEX_TARGET( Flex fastq-lex.l ${CMAKE_CURRENT_SOURCE_DIR}/zz_fastq-lex.c
        COMPILE_FLAGS "--debug --noline" )
    set( Scanner ${FLEX_Flex_OUTPUTS} )
    BISON_TARGET( Grammar fastq-grammar.y ${CMAKE_CURRENT_SOURCE_DIR}/zz_fastq-grammar.c
        COMPILE_FLAGS "-Wno-other --no-lines -r state -v --defines=${CMAKE_CURRENT_SOURCE_DIR}/zz_fastq-grammar.h" )
    set( Parser ${BISON_Grammar_OUTPUT_SOURCE} )
    ADD_FLEX_BISON_DEPENDENCY(Flex Grammar)
    SET_DIRECTORY_PROPERTIES(PROPERTIES CLEAN_NO_CUSTOM 1)
else()
    set( Scanner zz_fastq-lex.c )
    set( Parser zz_fastq-grammar.c )
endif()

if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}")
	SET_SOURCE_FILES_PROPERTIES(${Scanner} PROPERTIES COMPILE_FLAGS -Wno-misleading-indentation)
endif()

set( FASTQ_SRC
	spot-assembler
	common-writer
	sequence-writer
	common-reader
	fastq-reader
	${Parser}
	${Scanner}
	id2name
)

# Internal
# libfastqloader
if( NOT WIN32 ) # sys/mman.h
	GenerateStaticLibsWithDefs( fastqloader "${FASTQ_SRC}" "" "${CMAKE_SOURCE_DIR}/libs/inc" )

# External
	GenerateExecutableWithDefs( latf-load "${FASTQ_SRC};fastq-loader;loader-imp" "" "${CMAKE_SOURCE_DIR}/libs/inc" "" )

	target_link_libraries( latf-load loader kapp ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
    if( TARGET latf-load-asan )
		target_link_libraries( latf-load-asan loader-asan kapp ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
	endif()
    if( TARGET latf-load-tsan )
		target_link_libraries( latf-load-tsan loader-tsan kapp ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
	endif()
	MakeLinksExe( latf-load false )
    add_dependencies( latf-load loader fastqloader )
endif()
