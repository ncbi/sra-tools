if( NOT WIN32 )

    find_package( HDF5 COMPONENTS C )

    # External
    if( HDF5_FOUND )
        add_compile_definitions( __mod__="tools/pacbio-load" )
        add_executable( pacbio-load
            pl-context
            pl-tools
            pl-zmw
            pl-basecalls_cmn
            pl-sequence
            pl-consensus
            pl-passes
            pl-metrics
            pl-regions
            pl-progress
            pacbio-load
        )

        # set( PACBIO_LOAD_LIB
        # 	kapp
        # 	tk-version
        # 	kdf5
        # 	load
        # 	${HDF5_LIBRARIES}
        # 	${COMMON_LIBS_WRITE}
        # )

        target_link_libraries( pacbio-load ${NCBI_VDB_LIBDIR}/libkdf5.${STLX} ${HDF5_LIBRARIES} ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
        MakeLinksExe( pacbio-load false )
    endif()

endif()