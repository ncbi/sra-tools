### ref-variation

add_executable ( ref-variation common.cpp helper.cpp ref-variation.cpp  )

# add_dependencies ( ref-variation ngs-vdb )

target_link_libraries ( ref-variation
    tk-version
    kapp
    ngs-vdb
    ncbi-ngs
    loader
	${COMMON_LIBS_READ}
)

MakeLinksExe ( ref-variation false )

### var-expand

add_executable ( var-expand common.cpp helper.cpp var-expand.cpp )

target_link_libraries ( var-expand
    tk-version
    kapp
    ngs-vdb
    ncbi-ngs
    loader
	${COMMON_LIBS_READ}
)

MakeLinksExe ( var-expand false )

### common

include_directories ( ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libs/inc )

# linker flags
if (WIN32)
    set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartup")
endif()

add_subdirectory ( test )

