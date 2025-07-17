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

    if( HDF5_LIBDIR )
        find_library( HDF5_LIBRARIES libhdf5.a HINTS ${HDF5_LIBDIR} )
        if ( HDF5_LIBRARIES )
            set( HDF5_FOUND true )
            if ( HDF5_INCDIR )
                set( HDF5_INCLUDE_DIR ${HDF5_INCDIR} )
                include_directories( ${HDF5_INCLUDE_DIR} )
                message( HDF5_INCLUDE_DIR=${HDF5_INCLUDE_DIR} )
                message( HDF5_LIBRARIES=${HDF5_LIBRARIES} )
            endif()
        endif()
    else()
        find_package( HDF5 COMPONENTS C )
    endif()

    if( HDF5_FOUND )

        set( SRC
            hdf5/hdf5arrayfile.c
            hdf5/hdf5dir.c
            hdf5/hdf5file.c
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

        set( LIBS kapp loader ${HDF5_LIBRARIES} ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DH5_USE_110_API" )
        GenerateExecutableWithDefs( pacbio-load "${SRC}" "__mod__=\"tools/pacbio-load\"" "${CMAKE_CURRENT_SOURCE_DIR};${HDF5_C_INCLUDE_DIRS}" "${LIBS}" )

        MakeLinksExe( pacbio-load false )
    endif()

endif()

