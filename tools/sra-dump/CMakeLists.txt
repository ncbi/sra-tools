add_compile_definitions( __mod__="tools/sra-dump" )

set( DUMP_COMMON_SRC
	factory
	fasta_dump
	core
)

# External
add_executable( fastq-dump ${DUMP_COMMON_SRC} fastq )
target_link_libraries( fastq-dump ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( fastq-dump true )

add_executable( sff-dump ${DUMP_COMMON_SRC} sff )
target_link_libraries( sff-dump ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( sff-dump false )

add_executable( illumina-dump ${DUMP_COMMON_SRC} illumina )
target_link_libraries( illumina-dump ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( illumina-dump false )

add_executable( abi-dump ${DUMP_COMMON_SRC} abi )
target_link_libraries( abi-dump ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( abi-dump false )
