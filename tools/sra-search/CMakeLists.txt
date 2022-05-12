if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
endif ()

add_executable ( sra-search
    main.cpp
    searchblock.cpp
    fragmentmatchiterator.cpp
    blobmatchiterator.cpp
    referencematchiterator.cpp
    vdb-search.cpp
)

target_include_directories ( sra-search PRIVATE ${CMAKE_SOURCE_DIR}/libs/inc ${CMAKE_SOURCE_DIR}/ngs )

add_dependencies ( sra-search ngs-vdb ncbi-ngs tk-version )

target_link_libraries ( sra-search
    ngs-vdb
    ncbi-ngs
    tk-version
    ${CMAKE_STATIC_LIBRARY_PREFIX}kapp${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${COMMON_LIBS_READ}
    ${COMMON_LINK_LIBRARIES}
)

MakeLinksExe ( sra-search false )

if ( NOT TOOLS_ONLY )
    add_subdirectory( test )
endif()
