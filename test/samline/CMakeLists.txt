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

if( NOT WIN32 )

    # specify the location of schema files in a local .kfg file, to be used by the tests here as needed
    #add_test(NAME LatfTestSetup COMMAND sh -c "echo 'vdb/schema/paths = \"${VDB_INCDIR}\"\n/LIBS/GUID=\"8test002-6ab7-41b2-bfd0-latfload\"' > tmp.kfg" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # build the 2 helper-tools used in scripted tests:
    add_executable( samline refbases.c cigar.c alig-gen.c )
    add_executable( sampart refbases.c cigar.c sampart.c )

    # link agains libs 
    target_link_libraries( samline ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
    target_link_libraries( sampart ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )

    #add_test( NAME Test_Samline
    #        COMMAND
    #            ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
    #            ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
    #            "./simple_test.sh" "${BINDIR}" "${TESTBINDIR}"
    #        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    #    )

    #add_test( NAME Test_Samline
    #        COMMAND "./simple_test.sh" "${BINDIR}" "${TESTBINDIR}"
    #        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    #    )

    add_test( NAME Test_SamBuild
            COMMAND "./1.sh" "${BINDIR}" "${TESTBINDIR}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    
endif()
