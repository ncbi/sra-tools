add_compile_definitions( __mod__="tools/vdb-config" )


if (WIN32)
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartupNoPathConversion" )
	set( TUI_LIBS tui_cpp tui)
elseif( SINGLE_CONFIG)
	set( TUI_LIBS ${NCBI_VDB_LIBDIR}/libtui_cpp.${STLX} ${NCBI_VDB_LIBDIR}/libtui.${STLX})
else() # XCode
	set( TUI_LIBS tui_cpp tui)
endif()

add_executable( vdb-config
	configure
	util
	vdb-config
	interactive
	interactive2
	vdb-config-model
)
target_link_libraries ( vdb-config ${TUI_LIBS} ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-config false )

add_custom_command( TARGET vdb-config POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${VDB_SRCDIR}/libs/kfg/default.kfg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ncbi/default.kfg
)
