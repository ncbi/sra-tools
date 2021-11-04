if( Python3_EXECUTABLE )
    if( SINGLE_CONFIG )
        add_test( NAME SlowTest_NgsPython
            COMMAND ${Python3_EXECUTABLE} tests.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        set_tests_properties( SlowTest_NgsPython
            PROPERTIES
                DEPENDS Test_NGS_Python_install
                ENVIRONMENT "NGS_PY_DOWNLOAD_LIBRARY=0;PYTHONUSERBASE=${PythonUserBase};NGS_PY_LIBRARY_PATH=${LIBDIR};LD_LIBRARY_PATH=${NCBI_VDB_LIBDIR}"
        )
    elseif( WIN32 )
        add_test( NAME SlowTest_NgsPython
            COMMAND ${Python3_EXECUTABLE} tests.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        set_tests_properties( SlowTest_NgsPython
            PROPERTIES
                DEPENDS Test_NGS_Python_install
                ENVIRONMENT "NGS_PY_DOWNLOAD_LIBRARY=0;PYTHONUSERBASE=${PythonUserBase};NGS_PY_LIBRARY_PATH=${LIBDIR}/../bin"
        )
    endif()
endif()
