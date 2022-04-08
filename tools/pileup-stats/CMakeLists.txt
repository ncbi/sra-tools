add_executable ( pileup-stats
    pileup-stats
)

include_directories ( ${CMAKE_SOURCE_DIR}/ngs ${CMAKE_SOURCE_DIR}/libs/inc )

add_dependencies ( pileup-stats general-writer )

target_link_libraries ( pileup-stats
    general-writer
    ncbi-ngs
	${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE}
)

MakeLinksExe ( pileup-stats false )

add_subdirectory ( test )
