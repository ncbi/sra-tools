add_compile_definitions( __mod__="tools/sra-stat" )

if( WIN32 )
	set( ADDITIONAL_LIBRARIES )
else()
	set( ADDITIONAL_LIBRARIES m )
endif()

# External
add_executable( sra-stat
	assembly-statistics
	sra
	sra-stat
)
target_link_libraries( sra-stat ${COMMON_LINK_LIBRARIES} load ncbi-vdb ${ADDITIONAL_LIBRARIES} )
