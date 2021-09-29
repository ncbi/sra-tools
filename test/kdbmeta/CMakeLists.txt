if ( NOT WIN32 )
    add_test( NAME SlowTest_Kdbmeta
          COMMAND bash -c "${BINDIR}/kdbmeta SRR053325 > /dev/null"
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
else()
#TODO: make run on Windows
endif()
