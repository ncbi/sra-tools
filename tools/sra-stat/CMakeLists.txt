add_compile_definitions( __mod__="tools/sra-stat" )

# External
add_executable( sra-stat
	assembly-statistics
	sra
	sra-stat
)
target_link_libraries( sra-stat ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( sra-stat )
