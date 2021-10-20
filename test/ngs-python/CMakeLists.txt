if( Python3_EXECUTABLE )

    add_test( NAME SlowTest_NgsPython
        COMMAND ${Python3_EXECUTABLE} tests.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( SlowTest_NgsPython
        PROPERTIES
            DEPENDS Test_NGS_Python_install
            ENVIRONMENT "PYTHONUSERBASE=${TEMPDIR}/ngs;NGS_PY_LIBRARY_PATH=${LIBDIR}"
    )

endif()