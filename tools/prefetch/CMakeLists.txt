add_compile_definitions( __mod__="tools/prefetch" )

if (WIN32)
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartupNoPathConversion" )
endif()

add_executable( prefetch
	kfile-no-q
	PrfMain
	prefetch
	PrfRetrier
	PrfOutFile
)
target_link_libraries ( prefetch ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( prefetch true )
