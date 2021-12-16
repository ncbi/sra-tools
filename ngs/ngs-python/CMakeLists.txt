if( Python3_EXECUTABLE )

    add_subdirectory( examples )

    #TODO: install

    add_test(
        NAME Test_NGS_Python_install
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/setup.py  -q install --user
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties( Test_NGS_Python_install
        PROPERTIES ENVIRONMENT "PYTHONUSERBASE=${PythonUserBase}"
    )

endif()
