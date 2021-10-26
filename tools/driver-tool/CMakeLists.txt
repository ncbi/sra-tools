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
    support2
)

if( WIN32 )
    set( ADDITIONAL_LIBS
            libcmt$<$<CONFIG:Debug>:d>
            libcpmt$<$<CONFIG:Debug>:d>
            libucrt$<$<CONFIG:Debug>:d>
            libvcruntime$<$<CONFIG:Debug>:d>
    )
else()
	set( ADDITIONAL_LIBS m )
endif()

add_executable( sratools "${SRATOOLS_SRC}" )
target_link_libraries( sratools ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} json secure utf8proc ${ADDITIONAL_LIBS} )
MakeLinksExe( sratools true )
