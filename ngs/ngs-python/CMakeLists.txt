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

    add_subdirectory( examples )

    #TODO: install

    execute_process (COMMAND "${Python3_EXECUTABLE}" -m venv "${CMAKE_BINARY_DIR}/venv")
    # update the environment with VIRTUAL_ENV variable (mimic the activate script)
    set (ENV{VIRTUAL_ENV} "${CMAKE_BINARY_DIR}/venv")
    # change the context of the search
    set (Python3_FIND_VIRTUALENV FIRST)
    # unset Python3_EXECUTABLE because it is also an input variable
    # (see documentation, Artifacts Specification section)
    unset (Python3_EXECUTABLE)
    # Launch a new search
    find_package (Python3 COMPONENTS Interpreter Development)
    # message(STATUS "Use: ${Python3_EXECUTABLE}")

    execute_process (COMMAND "${Python3_EXECUTABLE}" -m pip install --upgrade pip setuptools wheel)

    add_test(
        NAME Test_NGS_Python_install
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py  -q install --user
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties( Test_NGS_Python_install
        PROPERTIES ENVIRONMENT "PYTHONUSERBASE=${PythonUserBase}"
    )

endif()
