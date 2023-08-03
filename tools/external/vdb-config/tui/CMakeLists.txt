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

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. )

if ( "windows" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC win/systui.c )
elseif( "linux" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC linux/systui.c )
    if ( CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" )
        set_source_files_properties( linux/systui.c PROPERTIES COMPILE_FLAGS -D_XOPEN_SOURCE=700 )
    endif()
elseif( "mac" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC bsd/systui.c )
endif()

set( SRC
    dir_dlg.c
    eventring.c
    file_dlg.c
    line_policy.c
    screen.c
    string_cache.c
    tui.c
    tui_dlg.c
    tui_dlg_helper.c
    tui_menu.c
    tui_palette.c
    tui_std_dlg.c
    tui_widget.c
    tui_widget_button.c
    tui_widget_checkbox.c
    tui_widget_grid.c
    tui_widget_inputline.c
    tui_widget_label.c
    tui_widget_progress.c
    tui_widget_radiobox.c
    tui_widget_spin_edit.c
    tui_widget_string_list.c
    tui_widget_tabhdr.c
    ${OS_SPECIFIC_SRC}
)

GenerateStaticLibs( tui "${SRC}" )
GenerateStaticLibs( tui_cpp tui_cpp )

add_compile_definitions (__mod__="libs/tui")
