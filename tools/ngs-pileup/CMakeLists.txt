add_compile_definitions( __mod__="tools/ngs-pileup" )

include_directories( ${CMAKE_SOURCE_DIR} )

# Internal
add_executable( ngs-pileup ngs-pileup main )

if( WIN32 )
	set( ADDITIONAL_LIBS Ws2_32 Crypt32 )
else()
	set( ADDITIONAL_LIBS )
endif()

set( NGS_PILEUP_LIBS
	ncbi-ngs-c++
	ngs-c++
	ngs
	ncbi-vdb
	kapp
	tk-version
)

target_link_libraries( ngs-pileup ${NGS_PILEUP_LIBS} ${ADDITIONAL_LIBS} )
MSVS_StaticRuntime( ngs-pileup )