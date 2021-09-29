add_compile_definitions( __mod__="tools/prefetch" )

add_executable( prefetch
	kfile-no-q
	PrfMain
	prefetch
	PrfRetrier
	PrfOutFile
)
target_link_libraries ( prefetch ${COMMON_LINK_LIBRARIES} ncbi-vdb )
