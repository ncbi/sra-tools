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
add_compile_definitions( __mod__="test/external/vdb-validate" )

# unit tests

if( NOT DEFINED VDB_SRCDIR )
    set( VDB_SRCDIR "${VDB_INCDIR}/.." )
endif()

AddExecutableTest( Test_VdbValidate_Unit "test-vdb-validate"
                                "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}"
                                "${PROJECT_SOURCE_DIR}/tools/external/vdb-validate" )


if ( NOT WIN32 )
	ToolsRequired(vdb-validate prefetch)

	add_test( NAME Test_Vdb_validate_dflt
		COMMAND vdb_validate.sh "${DIRTOTEST}" vdb-validate
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_Vdb_validate_redact
        COMMAND sh -c "./vdb_validate_redact.sh '${DIRTOTEST}' '${VDB_LIBDIR}' '${CMAKE_SOURCE_DIR}/libs/schema:${VDB_INCDIR}' '${VDB_SRCDIR}/py_vdb'"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	if( TARGET vdb-validate-asan )
		add_test( NAME Test_Vdb_validate-asan
			COMMAND vdb_validate.sh "${DIRTOTEST}" vdb-validate-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET vdb-validate-tsan )
		add_test( NAME Test_Vdb_validate-tsan
			COMMAND vdb_validate.sh "${DIRTOTEST}" vdb-validate-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

endif()
