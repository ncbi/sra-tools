add_compile_definitions( __mod__="tools/kqsh" )

# External
add_executable( kqsh
)
target_link_libraries( kqsh ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( kqsh )