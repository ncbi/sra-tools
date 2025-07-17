if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
endif ()

set( SRC
    main.cpp
    searchblock.cpp
    fragmentmatchiterator.cpp
    blobmatchiterator.cpp
    referencematchiterator.cpp
    vdb-search.cpp
)

set( LIBS
    ngs-vdb
    ncbi-ngs
    tk-version
    kapp
    ${COMMON_LIBS_READ}
    ${COMMON_LINK_LIBRARIES}
    ksrch
)

GenerateExecutableWithDefs( sra-search "${SRC}" "__mod__=\"tools/sra-search\"" "${CMAKE_SOURCE_DIR}/libs/inc;${CMAKE_SOURCE_DIR}/ngs" "${LIBS}" )

add_dependencies ( sra-search ngs-vdb ncbi-ngs tk-version )
if( TARGET sra-search-asan )
    add_dependencies ( sra-search-asan ngs-vdb ncbi-ngs tk-version )
endif()
if( TARGET sra-search-tsan )
    add_dependencies ( sra-search-tsan ngs-vdb ncbi-ngs tk-version )
endif()
MakeLinksExe ( sra-search false )

if ( NOT TOOLS_ONLY )
    add_subdirectory( test )
endif()
