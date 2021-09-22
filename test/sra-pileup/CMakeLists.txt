add_compile_definitions( __mod__="test/sra-pileup" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_SraPileup_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${CMAKE_BINARY_DIR}/bin/sra-pileup
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()


if ( NOT WIN32 AND Python3_EXECUTABLE )
	add_test( NAME SlowTest_SraPileup
		COMMAND runtests-slow.sh ${CMAKE_BINARY_DIR}/bin "${Python3_EXECUTABLE}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()