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

include_directories( ${CMAKE_SOURCE_DIR} )
set( NGS_PILEUP_LIBS
    ncbi-ngs-c++
    ngs-c++
    ngs
    ${COMMON_LINK_LIBRARIES}
    ${COMMON_LIBS_READ}
)
AddExecutableTest( Test_Ngs_Pileup "test-ngs-pileup.cpp" "${NGS_PILEUP_LIBS}" "" )

if( NOT WIN32 )
    ToolsRequired(ngs-pileup)

    #TODO: fix
    # add_test( NAME Test_Ngs_Pileup_1
    #           COMMAND ./runtestcase.sh ${BINDIR} ${CMAKE_CURRENT_SOURCE_DIR} 1.0 SRR833251
    #           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_Ngs_Pileup_2
            COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 2.0 SRR1166393
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET Test_Ngs_Pileup-asan )
        add_test( NAME Test_Ngs_Pileup_2-asan
                COMMAND ./runtestcase.sh ${BINDIR} "-asan" ${CMAKE_CURRENT_SOURCE_DIR} 2.0 SRR1166393
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET Test_Ngs_Pileup-tsan )
        add_test( NAME Test_Ngs_Pileup_2-tsan
                COMMAND ./runtestcase.sh ${BINDIR} "-tsan" ${CMAKE_CURRENT_SOURCE_DIR} 2.0 SRR1166393
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    # Illumina
    add_test( NAME Test_Ngs_Pileup_3
            COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 3.0 ERR334733
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET Test_Ngs_Pileup-asan )
        add_test( NAME Test_Ngs_Pileup_3-asan
                COMMAND ./runtestcase.sh ${BINDIR} "-asan" ${CMAKE_CURRENT_SOURCE_DIR} 3.0 ERR334733
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET Test_Ngs_Pileup-tsan )
        add_test( NAME Test_Ngs_Pileup_3-tsan
                COMMAND ./runtestcase.sh ${BINDIR} "-tsan" ${CMAKE_CURRENT_SOURCE_DIR} 3.0 ERR334733
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    add_test( NAME Test_Ngs_Pileup_4
            COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 4.0 SRR1068006
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET Test_Ngs_Pileup-asan )
        add_test( NAME Test_Ngs_Pileup_4-asan
                COMMAND ./runtestcase.sh ${BINDIR} "-asan" ${CMAKE_CURRENT_SOURCE_DIR} 4.0 SRR1068006
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET Test_Ngs_Pileup-tsan )
        add_test( NAME Test_Ngs_Pileup_4-tsan
                COMMAND ./runtestcase.sh ${BINDIR} "-tsan" ${CMAKE_CURRENT_SOURCE_DIR} 4.0 SRR1068006
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    # ABSOLID REF_ROWS=36172
    add_test( NAME SlowTest_Ngs_Pileup_5
            COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 5.0 SRR619510
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET Test_Ngs_Pileup-asan )
        add_test( NAME SlowTest_Ngs_Pileup_5-asan
                COMMAND ./runtestcase.sh ${BINDIR} "-asan" ${CMAKE_CURRENT_SOURCE_DIR} 5.0 SRR619510
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET Test_Ngs_Pileup-tsan )
        add_test( NAME SlowTest_Ngs_Pileup_5-tsan
                COMMAND ./runtestcase.sh ${BINDIR} "-tsan" ${CMAKE_CURRENT_SOURCE_DIR} 5.0 SRR619510
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    #TODO: fix
    # SEC_ROWS=198
    # add_test( NAME Test_Ngs_Pileup_6
    #           COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 6.0 SRR1251988
    #           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # ION_TORRENT
    add_test( NAME Test_Ngs_Pileup_7
            COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 7.0 SRR1186012
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET Test_Ngs_Pileup-asan )
        add_test( NAME Test_Ngs_Pileup_7-asan
                COMMAND ./runtestcase.sh ${BINDIR} "-asan" ${CMAKE_CURRENT_SOURCE_DIR} 7.0 SRR1186012
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET Test_Ngs_Pileup-tsan )
        add_test( NAME Test_Ngs_Pileup_7-tsan
                COMMAND ./runtestcase.sh ${BINDIR} "-tsan" ${CMAKE_CURRENT_SOURCE_DIR} 7.0 SRR1186012
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    #TODO: fix
    # COMPLETE_GENOMICS
    # add_test( NAME Test_Ngs_Pileup_8.1
    #           COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 8.1 SRR556739 -r chrY
    #           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    #TODO: fix
    # COMPLETE_GENOMICS, circular reference
    # add_test( NAME Test_Ngs_Pileup_8.2
    #           COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 8.2 SRR556739 -r chrM
    #           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    #:19900-20022
    add_test( NAME Test_Ngs_Pileup_9
            COMMAND ./runtestcase.sh ${BINDIR} "" ${CMAKE_CURRENT_SOURCE_DIR} 9.0 SRR341578 -r NC_011752.1
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET Test_Ngs_Pileup-asan )
        add_test( NAME Test_Ngs_Pileup_9-asan
                COMMAND ./runtestcase.sh ${BINDIR} "-asan" ${CMAKE_CURRENT_SOURCE_DIR} 9.0 SRR341578 -r NC_011752.1
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET Test_Ngs_Pileup-tsan )
        add_test( NAME Test_Ngs_Pileup_9-tsan
                COMMAND ./runtestcase.sh ${BINDIR} "-tsan" ${CMAKE_CURRENT_SOURCE_DIR} 9.0 SRR341578 -r NC_011752.1
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
endif()