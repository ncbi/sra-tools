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

if( Python3_EXECUTABLE )

    set( SOURCES
        AlignTest.py
        AlignSliceTest.py
        FragTest.py
        PileupTest.py
        RefTest.py
    )

    if ( SINGLE_CONFIG )
      if ( NOT LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu" )
        if ( NOT TOOLS_ONLY )
            macro( RunAndDiff TestName TestParams )
                add_test( NAME Test_NgsPythonExamples_${TestName}
                    COMMAND
                        sh -c "NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${Python3_EXECUTABLE} ${TestName}.py ${TestParams}  > actual/${TestName} && diff expected/${TestName} actual/${TestName}"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                )
                set_tests_properties( Test_NgsPythonExamples_${TestName}
                    PROPERTIES ENVIRONMENT "NGS_PY_DOWNLOAD_LIBRARY=0;NGS_PY_LIBRARY_PATH=${LIBDIR}"
                    DEPENDS Test_NGS_Python_install
                )
            endmacro()

            RunAndDiff( FragTest "ERR225922 10000 2" )
            RunAndDiff( AlignTest "ERR225922 10000 2" )
            RunAndDiff( AlignSliceTest "SRR1121656 1 1 9999" )
            RunAndDiff( PileupTest "SRR1121656 1 9999 10003" )
            RunAndDiff( RefTest "SRR1121656" )
        endif()

        set( INSTALL_DEST ${CMAKE_INSTALL_PREFIX}/share/examples-python )

        install(FILES ${SOURCES} expected.txt
            DESTINATION ${INSTALL_DEST} )

        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.install
            DESTINATION ${INSTALL_DEST}
            RENAME Makefile
        )
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.install
            DESTINATION ${INSTALL_DEST}
            RENAME README.txt
        )
      endif()
    endif()

endif()
