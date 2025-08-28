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

add_compile_definitions( __mod__="test/internal/align-cache" )


if ( NOT WIN32 )

    ToolsRequired( align-cache vdb-validate )
    add_test( NAME Test_Align_Cache
          COMMAND runtest.sh ${SRC_INTERFACES_DIR}:${VDB_INTERFACES_DIR} ${DIRTOTEST} align-cache
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
# see VDB-6028
#     if( TARGET align-cache-asan )
#         add_test( NAME Test_Align_Cache-asan
#               COMMAND runtest.sh ${SRC_INTERFACES_DIR}:${VDB_INTERFACES_DIR} ${BINDIR} align-cache-asan
#               WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
#     endif()
    if( TARGET align-cache-tsan )
        add_test( NAME Test_Align_Cache-tsan
              COMMAND runtest.sh ${SRC_INTERFACES_DIR}:${VDB_INTERFACES_DIR} ${BINDIR} align-cache-tsan
              WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    #TODO: make run on Windows
endif()

# vg: $(DIRTOTEST)/align-cache
# 	valgrind --ncbi --suppressions=$(SRCDIR)/valgrind.suppress \
# 	 $(DIRTOTEST)/align-cache -t 10 --min-cache-count 1 CSRA_file CSRA_file.cache
