add_compile_definitions( __mod__="tools/kar" )

# External
add_executable( kar
	kar-path
	kar-args
	kar
)
target_link_libraries( kar ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )

add_executable( kar+
	kar-path
	kar+args
	kar+print
	kar+util
	kar+
)
target_link_libraries( kar+ ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )

add_executable( kar+meta
	kar+util
	kar+meta
)
target_link_libraries( kar+meta ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )

# # ?
# add_executable( sra-kar sra-kar )
# target_link_libraries( sra-kar ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m )
