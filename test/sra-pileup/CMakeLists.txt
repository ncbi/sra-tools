add_compile_definitions( __mod__="test/sra-pileup" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_SraPileup_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${BINDIR}/sra-pileup${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()


if ( NOT WIN32 AND Python3_EXECUTABLE )
	add_test( NAME SlowTest_SraPileup
		COMMAND runtests-slow.sh ${BINDIR} "${Python3_EXECUTABLE}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()
