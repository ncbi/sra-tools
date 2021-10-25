add_compile_definitions( __mod__="tools/vdb-validate" )

# External
add_executable( vdb-validate vdb-validate )
target_link_libraries( vdb-validate ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-validate true )

add_executable( check-corrupt check-corrupt )
target_link_libraries( check-corrupt ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( check-corrupt true )
