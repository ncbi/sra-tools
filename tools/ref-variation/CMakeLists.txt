# common flags
include_directories ( ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libs/inc )

link_libraries (
    tk-version
    kapp
    ngs-vdb
    ncbi-ngs
    loader
	${COMMON_LIBS_READ}
    ${COMMON_LINK_LIBRARIES}
)

if (WIN32)
    set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartup")
endif()

### ref-variation

GenerateExecutableWithDefs( ref-variation "common.cpp;helper.cpp;ref-variation.cpp" "__mod__=\"tools/ref-variation\"" "" "" )
# add_dependencies ( ref-variation ngs-vdb )

MakeLinksExe ( ref-variation false )

### var-expand

GenerateExecutableWithDefs( var-expand "common.cpp;helper.cpp;var-expand.cpp" "__mod__=\"tools/var-expand\"" "" "" )
MakeLinksExe ( var-expand false )

### common

if ( NOT TOOLS_ONLY )
    add_subdirectory( test )
endif()


