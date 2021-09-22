if( NOT WIN32 )

    if ( NOT TMPDIR )
        set( TMPDIR "/tmp" )
    endif()
    if ( NOT TEMPDIR )
        set( TEMPDIR ${TMPDIR} )
    endif()

    ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/test-data.kar
        COMMAND
            ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
            ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
            ./gendata.sh ${CMAKE_BINARY_DIR}/bin ${Python3_EXECUTABLE} ${TEMPDIR} ${CMAKE_CURRENT_SOURCE_DIR}/test-data.kar
        DEPENDS generate-test-data.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # To regenerate test-data, run 'make GenerateTestData' in the cmake build directory for this sub-project. This will re-create 'test-data.kar' and remove 'expected', thus breaking the test (Test_MakeReadFilter). To make the test pass again, copy /tmp/actual and /tmp/actual.stats to the source directory.
    ADD_CUSTOM_TARGET(GenerateTestData ALL
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test-data.kar
    )

    add_test( NAME Test_MakeReadFilter
        COMMAND
        ${CMAKE_COMMAND} -E env VDB_CONFIG=${TEMPDIR}
        ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
        ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
        ./runtest.sh ${CMAKE_BINARY_DIR}/bin ${VDB_LIBDIR} ${TEMPDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
#TODO: make work on Windows
endif()
