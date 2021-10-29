if ( Java_FOUND )

    include(UseJava)

    set( NGS_SRC
	        ngs_test_lib_load.java
	        ngs_test_CSRA1.java
    )

    set( NGS_JAVA_JAR ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-java.jar )
    set( JUNIT_JAR ${CMAKE_CURRENT_SOURCE_DIR}/junit-4.11.jar )
    set( CMAKE_JAVA_INCLUDE_PATH ${NGS_JAVA_JAR} ${JUNIT_JAR} )

    add_jar( test-ngs-java
        SOURCES ${NGS_SRC}
        INCLUDE_JARS ngs-java ${JUNIT_JAR}
        OUTPUT_DIR ${CMAKE_JAR_OUTPUT_DIRECTORY}
    )

    # use standard java System.loadLibrary
    set( VARS -Dvdb.System.loadLibrary=1 )

    # remove all found JNI DLL-s
    #set( VARS ${VARS} -Dvdb.deleteLibraries=1 )

    # custom load library search
    #set( VARS ${VARS} -Dvdb.loadLibraryLocations=W )

    set( VARS ${VARS} -Dvdb.log=FINEST )


    if ( NOT SINGLE_CONFIG )
        set( classpaths "${CMAKE_JAR_OUTPUT_DIRECTORY}/test-ngs-java.jar" "${NGS_JAVA_JAR}" "${JUNIT_JAR}" )
        cmake_path( CONVERT "${classpaths}" TO_NATIVE_PATH_LIST CLASSPATH )
        set( libpaths
                "${BINDIR}"
                "${LIBDIR}"

                "${NCBI_VDB_LIBDIR}"
                "$<$<CONFIG:Debug>:${NCBI_VDB_LIBDIR_DEBUG}>"
                "$<$<CONFIG:Release>:${NCBI_VDB_LIBDIR_RELEASE}>"
        )
        cmake_path( CONVERT "${libpaths}" TO_NATIVE_PATH_LIST LIBPATH )
    else()
        set( CLASSPATH "${CMAKE_JAR_OUTPUT_DIRECTORY}/test-ngs-java.jar:${NGS_JAVA_JAR}:${JUNIT_JAR}" )
        set( LIBPATH "${BINDIR}:${LIBDIR}:${NCBI_VDB_LIBDIR}" )
    endif()

    add_test( NAME Test_NgsJava_lib_load
        COMMAND ${Java_JAVA_EXECUTABLE} -cp "${CLASSPATH}" ${VARS} org.junit.runner.JUnitCore gov.nih.nlm.ncbi.ngs.ngs_test_lib_load
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test( NAME SlowTest_NgsJava_CSRA1
        COMMAND ${Java_JAVA_EXECUTABLE} -cp "${CLASSPATH}" "-Djava.library.path=${LIBPATH}" ${VARS} org.junit.runner.JUnitCore ngs_test_CSRA1
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()
