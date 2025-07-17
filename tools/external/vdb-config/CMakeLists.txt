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

set( TUI_LIBS tui_cpp tui)

add_subdirectory( tui )

add_compile_definitions( __mod__="tools/vdb-config" )

add_executable( vdb-config
	configure
	util
	vdb-config
	interactive2
	vdb-config-model
)

add_dependencies( vdb-config tui )
target_include_directories( vdb-config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries ( vdb-config ${TUI_LIBS} ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-config false )

add_custom_command( TARGET vdb-config POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${VDB_INCDIR}/kfg/ncbi/default.kfg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ncbi/default.kfg
)
