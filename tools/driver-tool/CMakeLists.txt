add_compile_definitions( __mod__="tools/driver-tool" )

add_subdirectory( json )
add_subdirectory( secure )
add_subdirectory( utf8proc )

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

if( WIN32 )
	set( ADDITIONAL_LIBS Ws2_32 Crypt32 )
else()
	set( ADDITIONAL_LIBS m )
endif()

#GenerateStaticLibs( sratools "${SRATOOLS_SRC}" )
add_executable( sratools "${SRATOOLS_SRC}" )
MSVS_StaticRuntime( sratools )
target_link_libraries( sratools ncbi-vdb json secure utf8proc ${ADDITIONAL_LIBS} )

if( WIN32 )
	add_executable( sratools-md "${SRATOOLS_SRC}" )
	MSVS_DLLRuntime( sratools-md )
	target_link_libraries( sratools-md ncbi-vdb json secure utf8proc ${ADDITIONAL_LIBS} )
endif()

#if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
#	target_compile_options(sratools INTERFACE "/Zc:__cplusplus")
#endif()
#add_compile_definitions( __cplusplus=201103 )
