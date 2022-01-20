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

if( NOT WIN32 )

    find_package( HDF5 COMPONENTS C )

    # External
    if( HDF5_FOUND )
        add_compile_definitions( __mod__="tools/pacbio-load" )
        add_executable( pacbio-load
            pl-context
            pl-tools
            pl-zmw
            pl-basecalls_cmn
            pl-sequence
            pl-consensus
            pl-passes
            pl-metrics
            pl-regions
            pl-progress
            pacbio-load
        )

        if ( SINGLE_CONFIG )
           target_link_libraries( pacbio-load ${NCBI_VDB_LIBDIR}/libkdf5.${STLX} )
        else()
           target_link_libraries( pacbio-load kdf5 )
        endif()
        target_link_libraries( pacbio-load ${HDF5_LIBRARIES} ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )

        MakeLinksExe( pacbio-load false )
    endif()

endif()
