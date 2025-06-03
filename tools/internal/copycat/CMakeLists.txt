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

add_compile_definitions( __mod__="tools/internal/copycat" )

# External
if( ${OS} STREQUAL "linux" )

    # always link against shared libmagic
    find_library( HAVE_MAGIC libmagic.so )

    if ( NOT HAVE_MAGIC STREQUAL "HAVE_MAGIC-NOTFOUND" )

        set( KFF_SRC
            kff/fileformat.c
            kff/ffext.c
            kff/ffkey.c
            kff/ffmagic.c
        )
        set( SRC
            copycat
            ccfileformat
            cccat
            cctree
            cctree-dump
            cctar
            ccsra
            ccsubchunk
            ccfile
            ../shared/teefile.c
            ${KFF_SRC}
        )
        GenerateExecutableWithDefs( copycat "${SRC}" "" "${CMAKE_CURRENT_SOURCE_DIR}" "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ};${HAVE_MAGIC}" )
        MakeLinksExe( copycat false )

    endif()

 endif()
