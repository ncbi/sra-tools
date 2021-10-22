add_compile_definitions( __mod__="tools/vdb-validate" )

if( WIN32 )
	set( ADDITIONAL_LIBS )
else()
	set( ADDITIONAL_LIBS m)
endif()

# External
add_executable( vdb-validate vdb-validate )
target_link_libraries( vdb-validate ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl ${ADDITIONAL_LIBS} )

add_executable( check-corrupt check-corrupt )
target_link_libraries( check-corrupt ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl )
