# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

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
        ./runtest.sh ${BINDIR} ${NCBI_VDB_LIBDIR} ${TEMPDIR} ""
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if( RUN_SANITIZER_TESTS )
        add_test( NAME Test_MakeReadFilter-asan
            COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${TEMPDIR}
            ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
            ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
            ./runtest.sh ${BINDIR} ${NCBI_VDB_LIBDIR} ${TEMPDIR} "-asan"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        add_test( NAME Test_MakeReadFilter-tsan
            COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${TEMPDIR}
            ${CMAKE_COMMAND} -E env NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg
            ${CMAKE_COMMAND} -E env VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR}
            ./runtest.sh ${BINDIR} ${NCBI_VDB_LIBDIR} ${TEMPDIR} "-tsan"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
else()
#TODO: make work on Windows/XCode
endif()
