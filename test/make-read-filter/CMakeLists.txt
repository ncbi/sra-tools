if( NOT WIN32 )

    if ( NOT TMPDIR )
        set( TMPDIR "/tmp" )
    endif()
    if ( NOT TEMPDIR )
        set( TEMPDIR ${TMPDIR} )
    endif()

    # add_test( NAME Test_MakeReadFilter
    #     COMMAND
    #     ${CMAKE_COMMAND} -E env VDB_CONFIG=${TEMPDIR}
    #     ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
    #     ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
    #     "bash" "-c" "echo '/LIBS/GUID = \"c1d99592-6ab7-41b2-bfd0-8aeba5ef8498\"' > ${TEMPDIR}/tmp.mkfg; echo '/sra/quality_type = \"raw_scores\"' >>${TEMPDIR}/tmp.mkfg; ${Python3_EXECUTABLE} generate-test-data.py ${TEMPDIR}/test-data &&
    #     ${CMAKE_BINARY_DIR}/bin/kar -d ${TEMPDIR}/test-data -fc test-data.kar && rm -rf ${TEMPDIR}/test-data expected"
    #     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    # )
else()
#TODO: make work on Windows
endif()
