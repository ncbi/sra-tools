set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory( ncbi )
add_subdirectory( ngs-sdk )
if ( Java_FOUND )
	add_subdirectory( ngs-java )
endif()
add_subdirectory( ngs-python )
