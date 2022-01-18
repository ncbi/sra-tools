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

add_compile_definitions( __mod__="tools/vcf-loader" )

if( FLEX_FOUND AND BISON_FOUND )
    FLEX_TARGET( Flex vcf-lex.l ${CMAKE_CURRENT_SOURCE_DIR}/vcf-lex.c
        COMPILE_FLAGS "--debug --noline" )
    set( Scanner ${FLEX_Flex_OUTPUTS} )
    BISON_TARGET( Grammar vcf-grammar.y ${CMAKE_CURRENT_SOURCE_DIR}/vcf-grammar.c
        COMPILE_FLAGS "-Wno-other --no-lines -r state" )
    set( Parser ${BISON_Grammar_OUTPUT_SOURCE} )
    ADD_FLEX_BISON_DEPENDENCY(Flex Grammar)
else()
    set( Scanner vcf-lex.c )
    set( Parser vcf-grammar.c )
endif()

# Internal

add_library( vcfloader
	${Scanner}
	${Parser}
	vcf-reader
	vcf-database
)
