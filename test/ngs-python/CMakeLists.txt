if( Python3_EXECUTABLE )

    add_test( NAME SlowTest_NgsPython
        COMMAND
            # PYTHONUSERBASE is for Windows only but won't hurt on other platforms
            ${CMAKE_COMMAND} -E env PYTHONUSERBASE="${BINDIR}\\tmp\\ngs"
            ${Python3_EXECUTABLE} tests.py ${TestParams}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND_EXPAND_LISTS
    )
    set_tests_properties( SlowTest_NgsPython
        PROPERTIES DEPENDS Test_NGS_Python_install
    )
endif()