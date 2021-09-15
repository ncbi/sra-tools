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
MSVS_StaticRuntime( vcfloader )
