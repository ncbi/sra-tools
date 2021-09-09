add_compile_definitions( __mod__="tools/driver-tool" )

include_directories( inc )

set( SRATOOLS_SRC
	sratools
	sratools2
	imp_fasterq_dump
	imp_fastq_dump
	imp_prefetch
	imp_sam_dump
	imp_sra_pileup
	imp_srapath
	imp_vdb_dump
	cmdline
	config
	proc
	run-source
	uuid
	service
)

GenerateStaticLibs( sratools "${SRATOOLS_SRC}" )
target_link_libraries( sratools ncbi-vdb json m )

#if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
#	target_compile_options(sratools INTERFACE "/Zc:__cplusplus")
#endif()
#add_compile_definitions( __cplusplus=201103 )
