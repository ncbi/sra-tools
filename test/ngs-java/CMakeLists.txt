if ( Java_FOUND )

    include(UseJava)

    set( NGS_SRC
	        ngs_test_lib_load.java
	        ngs_test_CSRA1.java
    )

    set( NGS_JAVA_JAR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/ngs-java.jar )
    set( JUNIT_JAR ${CMAKE_CURRENT_SOURCE_DIR}/junit-4.11.jar )
    set( CMAKE_JAVA_INCLUDE_PATH ${NGS_JAVA_JAR} ${JUNIT_JAR} )

    add_jar( test-ngs-java
        SOURCES ${NGS_SRC}
        OUTPUT_DIR ${CMAKE_TEST_OUTPUT_DIRECTORY}
    )

    # use standard java System.loadLibrary
    set( VARS -Dvdb.System.loadLibrary=1 )

    # remove all found JNI DLL-s
    #set( VARS ${VARS} -Dvdb.deleteLibraries=1 )

    # custom load library search
    #set( VARS ${VARS} -Dvdb.loadLibraryLocations=W )

    set( VARS ${VARS} -Dvdb.log=FINEST )

    set( JAVA_CMD ${Java_JAVA_EXECUTABLE} -cp ${CMAKE_TEST_OUTPUT_DIRECTORY}/test-ngs-java.jar:${NGS_JAVA_JAR}:${JUNIT_JAR} -Djava.library.path=${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${VARS} org.junit.runner.JUnitCore )

    add_test( NAME Test_NgsJava_lib_load
        COMMAND ${JAVA_CMD} gov.nih.nlm.ncbi.ngs.ngs_test_lib_load
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test( NAME SlowTest_NgsJava_CSRA1
        COMMAND ${JAVA_CMD} ngs_test_CSRA1
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()