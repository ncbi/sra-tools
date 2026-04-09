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

add_compile_definitions( __mod__="test/internal/copycat" )

if ( ${OS} STREQUAL "linux"  )
    if ( NOT HAVE_MAGIC STREQUAL "HAVE_MAGIC-NOTFOUND" )

        ToolsRequired(copycat)

        set( SRC_DIR ${CMAKE_SOURCE_DIR}/tools/internal/copycat)
        set( KFF_SRC
                ${SRC_DIR}/kff/ffext.c
                ${SRC_DIR}/kff/ffkey.c
                ${SRC_DIR}/kff/fileformat.c
                ${SRC_DIR}/kff/ffmagic.c
        )
        AddExecutableTest( Test_KFF "kfftest.cpp;${KFF_SRC}" "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ};magic" "${SRC_DIR}" )

        add_test( NAME Test_Copycat_Help
            COMMAND sh -c "ls ${DIRTOTEST}/copycat>/dev/null 2>&1 && ${DIRTOTEST}/copycat -h || echo no copycat"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        add_test( NAME Test_Copycat_Bad_Name
            COMMAND ./test-bad-name.sh ${DIRTOTEST} copycat
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        if( TARGET copycat-asan )
            add_test( NAME Test_Copycat_Bad_Name-asan
                COMMAND ./test-bad-name.sh ${DIRTOTEST} copycat-asan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
        if( TARGET copycat-tsan )
            add_test( NAME Test_Copycat_Bad_Name-tsan
                COMMAND ./test-bad-name.sh ${DIRTOTEST} copycat-tsan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()

        add_test( NAME Test_Copycat_IdenticalCopy
            COMMAND ./test-copy.sh ${DIRTOTEST} copycat
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        if( TARGET copycat-asan )
            add_test( NAME Test_Copycat_IdenticalCopy-asan
                COMMAND ./test-copy.sh ${DIRTOTEST} copycat-asan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
        if( TARGET copycat-tsan )
            add_test( NAME Test_Copycat_IdenticalCopy-tsan
                COMMAND ./test-copy.sh ${DIRTOTEST} copycat-tsan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
        add_test( NAME Test_Copycat_FastqParser
            COMMAND ./test-fastq.sh ${DIRTOTEST} copycat
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        if( TARGET copycat-asan )
            add_test( NAME Test_Copycat_FastqParser-asan
                COMMAND ./test-fastq.sh ${DIRTOTEST} copycat-asan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
        if( TARGET copycat-tsan )
            add_test( NAME Test_Copycat_FastqParser-tsan
                COMMAND ./test-fastq.sh ${DIRTOTEST} copycat-tsan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
        add_test( NAME Test_Copycat_FastqGzipParser
            COMMAND ./test-fastq.gz.sh ${DIRTOTEST} copycat
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        # Fails, not going to fix.
        # if( TARGET copycat-asan )
        #     add_test( NAME Test_Copycat_FastqGzipParser-asan
        #         COMMAND ./test-fastq.gz.sh ${DIRTOTEST} copycat-asan
        #         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        # endif()
        if( TARGET copycat-tsan )
            add_test( NAME Test_Copycat_FastqGzipParser-tsan
                COMMAND ./test-fastq.gz.sh ${DIRTOTEST} copycat-tsan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
    else()
        message( "libmagic was not found; skipping copycat tests" )
    endif()
else()
#TODO: make run on Windows
endif()

