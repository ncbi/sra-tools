if( WIN32 )
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
endif()

add_subdirectory( driver-tool )
add_subdirectory( util )
add_subdirectory( align-cache )
add_subdirectory( align-info )
add_subdirectory( rd-filter-redact )
add_subdirectory( prefetch )
add_subdirectory( vdb-config ) # TODO: fix DEBUG
# add_subdirectory( vcf-config ) # TODO: lex
add_subdirectory( kar )
add_subdirectory( copycat )
add_subdirectory( sra-sort ) # TODO: __file__ in ctx.h; sra-sort-cg
add_subdirectory( sra-pileup ) # TODO: ifdef NCBI
add_subdirectory( fuse ) # TODO: ifdef NCBI, xml
# add_subdirectory( fastq-loader ) # TODO: bison
add_subdirectory( kget )
add_subdirectory( vdb-dump )
add_subdirectory( vdb-copy ) # TODO: vdb-copy.kfg
add_subdirectory( qual-recalib-stat )
add_subdirectory( vdb-validate )
add_subdirectory( srapath )
