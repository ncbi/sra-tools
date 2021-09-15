add_compile_definitions( __mod__="tools/fastq-loader" )

if( FLEX_FOUND AND BISON_FOUND )
    FLEX_TARGET( Flex fastq-lex.l ${CMAKE_CURRENT_SOURCE_DIR}/fastq-lex.c
        COMPILE_FLAGS "--debug --noline" )
    set( Scanner ${FLEX_Flex_OUTPUTS} )
    BISON_TARGET( Grammar fastq-grammar.y ${CMAKE_CURRENT_SOURCE_DIR}/fastq-grammar.c
        COMPILE_FLAGS "-Wno-other --no-lines -r state" )
    set( Parser ${BISON_Grammar_OUTPUT_SOURCE} )
    ADD_FLEX_BISON_DEPENDENCY(Flex Grammar)
else()
    set( Scanner fastq-lex.c )
    set( Parser fastq-grammar.c )
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
add_library( fastqloader ${FASTQ_SRC} )
MSVS_StaticRuntime( fastqloader )

# External
add_executable( latf-load
	${FASTQ_SRC}
	fastq-loader
	loader-imp
)
target_link_libraries( latf-load ${COMMON_LINK_LIBRARIES} load ncbi-wvdb )
MSVS_StaticRuntime( latf-load )
