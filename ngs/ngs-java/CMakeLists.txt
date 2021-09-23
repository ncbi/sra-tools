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

    #$<$<CONFIG:Debug>:${CMAKE_JAR_OUTPUT_DIRECTORY_DEBUG}> $<$<CONFIG:Release>:${CMAKE_JAR_OUTPUT_DIRECTORY_RELEASE}> ) 
     add_jar( ngs-java
        SOURCES ${NGS_SRC} ${ITF_SRC} ${NCBI_SRC}
        OUTPUT_DIR ${CMAKE_JAR_OUTPUT_DIRECTORY} 
        GENERATE_NATIVE_HEADERS ngs-jni-headers DESTINATION ./jni
    )

    ########################################
    # examples
    add_subdirectory( examples )

    ########################################
    # javadoc
    create_javadoc( ngs-doc
        PACKAGES gov.nih.nlm.ncbi.ngs ngs
    )

endif()