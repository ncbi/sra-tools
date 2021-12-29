if ( Java_FOUND )

    include(UseJava)

    set( SOURCES
        AlignTest.java
        AlignSliceTest.java
        FragTest.java
        PileupTest.java
        RefTest.java
        ReadGroupTest.java
        DumpReferenceFASTA.java
    )

    add_jar( ngs-examples
        SOURCES ${SOURCES}
        INCLUDE_JARS ngs-java
        OUTPUT_DIR ${CMAKE_JAR_OUTPUT_DIRECTORY}
    )

    if ( SINGLE_CONFIG )
        macro( RunAndDiff TestName TestParams )
            add_test(
                NAME Test_NgsJavaExamples_${TestName}
                COMMAND bash -c
                    "${Java_JAVA_EXECUTABLE} -Dvdb.log=FINEST -cp ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-examples.jar:${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-java.jar -Djava.library.path=${CMAKE_LIBRARY_OUTPUT_DIRECTORY} examples.${TestName} ${TestParams} > actual/${TestName} && diff expected/${TestName} actual/${TestName}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMAND_EXPAND_LISTS
            )
        endmacro()

        RunAndDiff( FragTest "ERR225922 10000 2" )
        RunAndDiff( AlignTest "ERR225922 10000 2" )
        RunAndDiff( AlignSliceTest "SRR1121656 1 1 9999" )
        RunAndDiff( PileupTest "SRR1121656 1 9999 10003" )
        RunAndDiff( RefTest "SRR1121656" )
        RunAndDiff( ReadGroupTest "SRR1121656" )
        RunAndDiff( DumpReferenceFASTA "SRR520124 1" )

        set( INSTALL_DEST ${CMAKE_INSTALL_PREFIX}/share/examples-java )

        install(FILES ${SOURCES}
            DESTINATION ${INSTALL_DEST}/examples )
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.install
            DESTINATION ${INSTALL_DEST}
            RENAME Makefile
        )
        install(FILES README.install
            DESTINATION ${INSTALL_DEST}
            RENAME README.txt
        )
        install(FILES expected.txt
            DESTINATION ${INSTALL_DEST} )
    endif()

endif()
