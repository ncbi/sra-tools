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

    ########################################
    # ngs-java.jar

    # java API
    set( NGS_SRC
        ngs/ErrorMsg.java
        ngs/Statistics.java
        ngs/Fragment.java
        ngs/FragmentIterator.java
        ngs/Read.java
        ngs/ReadIterator.java
        ngs/ReadGroup.java
        ngs/ReadGroupIterator.java
        ngs/Alignment.java
        ngs/AlignmentIterator.java
        ngs/PileupEvent.java
        ngs/PileupEventIterator.java
        ngs/Pileup.java
        ngs/PileupIterator.java
        ngs/Reference.java
        ngs/ReferenceIterator.java
        ngs/ReadCollection.java
        ngs/Package.java
    )

    # java language bindings
    set( ITF_SRC
        ngs/itf/Refcount.java
        ngs/itf/StatisticsItf.java
        ngs/itf/FragmentItf.java
        ngs/itf/FragmentIteratorItf.java
        ngs/itf/ReadItf.java
        ngs/itf/ReadIteratorItf.java
        ngs/itf/ReadGroupItf.java
        ngs/itf/ReadGroupIteratorItf.java
        ngs/itf/AlignmentItf.java
        ngs/itf/AlignmentIteratorItf.java
        ngs/itf/PileupEventItf.java
        ngs/itf/PileupEventIteratorItf.java
        ngs/itf/PileupItf.java
        ngs/itf/PileupIteratorItf.java
        ngs/itf/ReferenceItf.java
        ngs/itf/ReferenceIteratorItf.java
        ngs/itf/ReadCollectionItf.java
    )

    # NCBI engine bindings
    set( NCBI_SRC
        gov/nih/nlm/ncbi/ngs/DownloadManager.java
        gov/nih/nlm/ncbi/ngs/FileCreator.java
        gov/nih/nlm/ncbi/ngs/HttpManager.java
        gov/nih/nlm/ncbi/ngs/LibDependencies.java
        gov/nih/nlm/ncbi/ngs/LibManager.java
        gov/nih/nlm/ncbi/ngs/LibPathIterator.java
        gov/nih/nlm/ncbi/ngs/LibVersionChecker.java
        gov/nih/nlm/ncbi/ngs/LMProperties.java
        gov/nih/nlm/ncbi/ngs/Logger.java
        gov/nih/nlm/ncbi/ngs/Manager.java
        gov/nih/nlm/ncbi/ngs/NGS.java
        gov/nih/nlm/ncbi/ngs/Version.java
        gov/nih/nlm/ncbi/ngs/error/LibraryLoadError.java
        gov/nih/nlm/ncbi/ngs/error/LibraryNotFoundError.java
        gov/nih/nlm/ncbi/ngs/error/LibraryIncompatibleVersionError.java
        gov/nih/nlm/ncbi/ngs/error/cause/ConnectionProblemCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/DownloadDisabledCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/InvalidLibraryCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/JvmErrorCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/LibraryLoadCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/OutdatedJarCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/PrereleaseReqLibCause.java
        gov/nih/nlm/ncbi/ngs/error/cause/UnsupportedArchCause.java
    )

    find_package(JNI)

    if ( JNI_FOUND )
        add_jar( ngs-java
            SOURCES ${NGS_SRC} ${ITF_SRC} ${NCBI_SRC}
            OUTPUT_DIR ${CMAKE_JAR_OUTPUT_DIRECTORY}
            GENERATE_NATIVE_HEADERS ngs-jni-headers DESTINATION ./jni
        )
    else()
        add_jar( ngs-java
            SOURCES ${NGS_SRC} ${ITF_SRC} ${NCBI_SRC}
            OUTPUT_DIR ${CMAKE_JAR_OUTPUT_DIRECTORY}
        )
    endif()

    install_jar( ngs-java ${CMAKE_INSTALL_PREFIX}/jar/ )
    #TODO: add symlinks

    ########################################
    # examples
    add_subdirectory( examples )
    ########################################
    # javadoc
    if ( Java_JAVADOC_EXECUTABLE )
        create_javadoc( ngs-doc
            PACKAGES gov.nih.nlm.ncbi.ngs ngs
            SOURCEPATH ${CMAKE_CURRENT_SOURCE_DIR}
        )

        foreach(f ${NGS_SRC})
            set(SRC_FILES "${SRC_FILES} ${f}")
        endforeach()

        foreach(f ${ITF_SRC})
            set(SRC_FILES "${SRC_FILES} ${f}")
        endforeach()

        foreach(f ${NCBI_SRC})
            set(SRC_FILES "${SRC_FILES} ${f}")
        endforeach()

        if( ${OS} STREQUAL "windows" )
            set( NGS_JAVADOC_DIR ${TARGDIR} )
        else()
            set( NGS_JAVADOC_DIR ${TARGDIR}/obj )
        endif()
        if ( Java_JAR_EXECUTABLE AND NOT (${OS} STREQUAL "windows") )
            add_custom_target(
                ngs-doc-jar ALL COMMAND
                bash -c "${Java_JAR_EXECUTABLE} -cfv ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-doc.jar ."
                DEPENDS ngs-doc_javadoc
                WORKING_DIRECTORY "${NGS_JAVADOC_DIR}/ngs/ngs-java/javadoc/ngs-doc"
            )
            install( FILES ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-doc.jar DESTINATION ${CMAKE_INSTALL_PREFIX}/jar/ )

            add_custom_target(
                ngs-src-jar ALL COMMAND
                bash -c "${Java_JAR_EXECUTABLE} -cfv ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-src.jar ${SRC_FILES}"
                DEPENDS ngs-doc_javadoc
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
            install( FILES ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-src.jar DESTINATION ${CMAKE_INSTALL_PREFIX}/jar/ )
        elseif ( Java_JAR_EXECUTABLE AND ${OS} STREQUAL "windows" )
            add_custom_target(
                ngs-doc-jar ALL COMMAND
                "${Java_JAR_EXECUTABLE}" -cfv "${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-doc.jar" .
                DEPENDS ngs-doc_javadoc
                WORKING_DIRECTORY "${NGS_JAVADOC_DIR}/ngs/ngs-java/javadoc/ngs-doc"
            )
            install( FILES ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-doc.jar DESTINATION ${CMAKE_INSTALL_PREFIX}/jar/ )

            add_custom_target(
                ngs-src-jar ALL COMMAND
                "${Java_JAR_EXECUTABLE}" -cfv "${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-src.jar" ${SRC_FILES}
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
            install( FILES ${CMAKE_JAR_OUTPUT_DIRECTORY}/ngs-src.jar DESTINATION ${CMAKE_INSTALL_PREFIX}/jar/ )
        endif()
    endif()
endif()
