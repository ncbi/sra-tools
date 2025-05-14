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

if( Python3_EXECUTABLE )
    if( SINGLE_CONFIG )
        if( NOT LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu" )
          add_test( NAME SlowTest_NgsPython
            COMMAND ${Python3_EXECUTABLE} tests.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          )
        else()
          add_test( NAME SlowTest_NgsPython
            COMMAND sh test-python.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          )
        endif()
        set_tests_properties( SlowTest_NgsPython
            PROPERTIES
                DEPENDS Test_NGS_Python_install
                ENVIRONMENT "LD_LIBRARY_PATH=${NCBI_VDB_LIBDIR};NCBI_VDB_QUALITY=R;NGS_PY_LIBRARY_PATH=${LIBDIR}"
        )

    elseif( WIN32 )
        add_test( NAME SlowTest_NgsPython
            COMMAND ${Python3_EXECUTABLE} tests.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        set_tests_properties( SlowTest_NgsPython
            PROPERTIES
                DEPENDS Test_NGS_Python_install
                ENVIRONMENT "NCBI_VDB_QUALITY=R;NGS_PY_LIBRARY_PATH=${BINDIR}"
        )
    endif()
endif()
