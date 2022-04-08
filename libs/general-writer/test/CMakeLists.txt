include_directories ( ${CMAKE_SOURCE_DIR}/libs/inc/general-writer )

add_executable ( test-general-writer test-general-writer.cpp )
add_dependencies ( test-general-writer general-writer )

target_link_libraries ( test-general-writer
    general-writer
    ${CMAKE_STATIC_LIBRARY_PREFIX}ktst${CMAKE_STATIC_LIBRARY_SUFFIX}
	${COMMON_LIBS_READ}
)

add_test ( NAME GeneralWriterTest
           COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-general-writer
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
