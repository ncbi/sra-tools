if( SINGLE_CONFIG )

    if ( ${OS} STREQUAL  "Linux" )
        ADD_CUSTOM_COMMAND(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/test-data.kar
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
                ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
                ./gendata.sh ${BINDIR} ${Python3_EXECUTABLE} ${TEMPDIR} ${CMAKE_CURRENT_SOURCE_DIR}/test-data.kar
            DEPENDS generate-test-data.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        # To regenerate test-data, run 'make GenerateTestData' in the cmake build directory for this sub-project. This will re-create 'test-data.kar' and remove 'expected', thus breaking the test (Test_MakeReadFilter). To make the test pass again, copy /tmp/actual and /tmp/actual.stats to the source directory.
        ADD_CUSTOM_TARGET(GenerateTestData
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test-data.kar
        )
    endif()

    add_test( NAME Test_MakeReadFilter
        COMMAND
        ${CMAKE_COMMAND} -E env VDB_CONFIG=${TEMPDIR}
        ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
        ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
        ./runtest.sh ${BINDIR} ${NCBI_VDB_LIBDIR} ${TEMPDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
#TODO: make work on Windows/XCode
endif()
