add_subdirectory( ngs )
add_subdirectory( ngs-c++ )
if ( Java_FOUND )
	add_subdirectory( ngs-jni )
endif()
add_subdirectory( ngs-py )

install( FILES NGS.hpp
         DESTINATION ${CMAKE_INSTALL_PREFIX}/include/ncbi
)