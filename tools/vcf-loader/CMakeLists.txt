add_compile_definitions( __mod__="tools/vcf-loader" )

if( FLEX_FOUND AND BISON_FOUND )
    FLEX_TARGET( Flex vcf-lex.l ${CMAKE_CURRENT_SOURCE_DIR}/zz_vcf-lex.c
        COMPILE_FLAGS "--debug --noline" )
    set( Scanner ${FLEX_Flex_OUTPUTS} )
    BISON_TARGET( Grammar vcf-grammar.y ${CMAKE_CURRENT_SOURCE_DIR}/zz_vcf-grammar.c
        COMPILE_FLAGS "-Wno-other --no-lines -r state -v --defines=${CMAKE_CURRENT_SOURCE_DIR}/zz_vcf-grammar.h" )
    set( Parser ${BISON_Grammar_OUTPUT_SOURCE} )
    ADD_FLEX_BISON_DEPENDENCY(Flex Grammar)
    SET_DIRECTORY_PROPERTIES(PROPERTIES CLEAN_NO_CUSTOM 1)
else()
    set( Scanner zz_vcf-lex.c )
    set( Parser zz_vcf-grammar.c )
endif()

# Internal

add_library( vcfloader
	${Scanner}
	${Parser}
	vcf-reader
	vcf-database
)
