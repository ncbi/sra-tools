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

    endif()
endif()
