add_compile_definitions( __mod__="tools/copycat" )

# External
if( ${OS} STREQUAL "linux" )
    add_executable( copycat
        copycat
        ccfileformat
        cccat
        cctree
        cctree-dump
        cctar
        ccsra
        ccsubchunk
        ccfile
    )
    target_link_libraries( copycat ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${NCBI_VDB_LIBDIR}/libkff.${STLX} ${NCBI_VDB_LIBDIR}/libmagic-static.${STLX} )
    MakeLinksExe( copycat false )

    # copy/install the magic file
    configure_file( magic ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/magic COPYONLY )
    install( FILES magic DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )

endif()
