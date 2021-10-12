if ( Java_FOUND )

    include(UseJava)

    add_jar( ngs-examples
        SOURCES
            AlignTest.java
            AlignSliceTest.java
            FragTest.java
            PileupTest.java
            RefTest.java
            ReadGroupTest.java
            DumpReferenceFASTA.java
        INCLUDE_JARS ngs-java
        OUTPUT_DIR ${LIBDIR}
    )

    if ( NOT WIN32 )
        macro( RunAndDiff TestName TestParams )
            add_test(
                NAME Test_NgsJavaExamples_${TestName}
                COMMAND bash -c
                    "${Java_JAVA_EXECUTABLE} -Dvdb.log=FINEST -cp ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-examples.jar:${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-java.jar -Djava.library.path=${CMAKE_LIBRARY_OUTPUT_DIRECTORY} examples.${TestName} ${TestParams} > actual/${TestName} && diff expected/${TestName} actual/${TestName}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endmacro()

        RunAndDiff( FragTest "ERR225922 10000 2" )
        RunAndDiff( AlignTest "ERR225922 10000 2" )
        RunAndDiff( AlignSliceTest "SRR1121656 1 1 9999" )
        RunAndDiff( PileupTest "SRR1121656 1 9999 10003" )
        RunAndDiff( RefTest "SRR1121656" )
        RunAndDiff( ReadGroupTest "SRR1121656" )
        RunAndDiff( DumpReferenceFASTA "SRR520124 1" )

    endif()
endif()