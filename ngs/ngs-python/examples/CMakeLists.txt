if( Python3_EXECUTABLE )

    if ( NOT WIN32 )
        macro( RunAndDiff TestName TestParams )
            add_test( NAME Test_NgsPythonExamples_${TestName}
                COMMAND
                bash -c "${Python3_EXECUTABLE} ${TestName}.py ${TestParams}  > actual/${TestName} && diff expected/${TestName} actual/${TestName}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
            set_tests_properties( Test_NgsPythonExamples_${TestName}
                PROPERTIES ENVIRONMENT "PYTHONUSERBASE=${TEMPDIR}/ngs"
            )
        endmacro()

        RunAndDiff( FragTest "ERR225922 10000 2" )
        RunAndDiff( AlignTest "ERR225922 10000 2" )
        RunAndDiff( AlignSliceTest "SRR1121656 1 1 9999" )
        RunAndDiff( PileupTest "SRR1121656 1 9999 10003" )
        RunAndDiff( RefTest "SRR1121656" )
    endif()

endif()