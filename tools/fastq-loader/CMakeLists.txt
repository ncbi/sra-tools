add_compile_definitions( __mod__="tools/fastq-loader" )

# TODO: bison

set( FASTQ_SRC
	spot-assembler
	common-writer
	sequence-writer
	common-reader
	fastq-reader
	fastq-grammar
	fastq-lex
	id2name
)

# Internal
libfastqloader

# External
add_executable( latf-load
	assembly-statistics
	sra
	sra-stat
)
target_link_libraries( latf-load ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( latf-load )
