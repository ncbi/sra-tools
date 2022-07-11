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
        COMMAND ${CMAKE_COMMAND} -E env
	    NCBI_VDB_QUALITY=R
	    ${Java_JAVA_EXECUTABLE} -cp "${CLASSPATH}" "-Djava.library.path=${LIBPATH}" ${VARS} org.junit.runner.JUnitCore ngs_test_CSRA1
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()
