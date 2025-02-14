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

# this is where test executables will be created
# to access tools themselves, use ${BINDIR}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TESTBINDIR} )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TESTBINDIR_DEBUG} )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TESTBINDIR_RELEASE} )

include_directories (".." )

add_executable ( test-general-loader
    test-general-loader
    testsource
)

add_dependencies ( test-general-loader general-writer gw-dumper )

target_link_libraries ( test-general-loader
    general-writer
    kapp
    ktst
    loader
    ${COMMON_LIBS_WRITE}
)

make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/db )
make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/input )
make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

#white box tests
add_test ( NAME GeneralLoaderWhiteBox
           COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-general-loader
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         )

# command line tests
add_executable ( makeinputs
    makeinputs
    testsource
)
target_link_libraries ( makeinputs
    general-writer
    ncbi-vdb-static
)

if ( NOT WIN32)
    #TODO: simplify
    set ( CMD sh runtestcase.sh "${DIRTOTEST} ${DIRTOTEST}/vdb-dump db" ${CMAKE_CURRENT_SOURCE_DIR} )

    file( WRITE ${CMAKE_CURRENT_SOURCE_DIR}/actual/t.mkfg
                "/vdb/schema/paths = \"${SRC_INTERFACES_DIR}:${VDB_INCDIR}\"\n/LIBS/GUID = \"c1d99592-6ab7-41b2-bfd0-8aeba5ef8498\"\n" )

    # FIXTURE
    add_test ( NAME makeinputs
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/makeinputs
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(makeinputs PROPERTIES FIXTURES_SETUP Fixture)
    #

    add_test ( NAME GeneralLoader-1.0-Basic-Unpacked
            COMMAND ${CMD} 1 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-1.1-Basic-Packed
            COMMAND ${CMD} 1packed 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-1.2-TargetDbOverride
            COMMAND ${CMD} 1override 0 "--target actual/1override/db"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-1.3-TargetDbOverrideShorthand
            COMMAND ${CMD} 1override 0 "-T actual/1override/db"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test ( NAME GeneralLoader-2.0-ErrorMessageEvent-Unpacked
            COMMAND ${CMD} 2 3 -L=err
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-2.1-ErrorMessageEvent-Packed
            COMMAND ${CMD} 2packed 3 -L=err
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test ( NAME GeneralLoader-3.0-EmptyDefaultValues-Unpacked
            COMMAND ${CMD} 3 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-3.1-EmptyDefaultValues-Packed
            COMMAND ${CMD} 3 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test ( NAME GeneralLoader-4.0-MoveAheadEvent-Unpacked
            COMMAND ${CMD} 4 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-4.1-MoveAheadEvent-Packed
            COMMAND ${CMD} 4packed 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test ( NAME GeneralLoader-5.0-IntegerConversion #packed only
            COMMAND ${CMD} 5packed 0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # tests depending on kdbmeta
    #TODO: simplify
    set ( CMD sh runtestcase.sh ${DIRTOTEST} "${DIRTOTEST}/kdbmeta db" ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test ( NAME GeneralLoader-6.0-SoftwareName-Unpacked
            COMMAND ${CMD} 6 0 -L=info SOFTWARE/formatter
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-6.1-SoftwareName-Packed
            COMMAND ${CMD} 6packed 0 -L=info SOFTWARE/formatter
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-7.0-MetadataAttrsDb-Unpacked
            COMMAND ${CMD} 7 0 -L=info dbmeta
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-7.1-MetadataAttrsDb-Packed
            COMMAND ${CMD} 7packed 0 -L=info dbmeta
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set ( CMD sh runtestcase.sh ${DIRTOTEST} "${DIRTOTEST}/kdbmeta db/tbl/REFERENCE" ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test ( NAME GeneralLoader-8.0-MetadataAttrsTbl-Unpacked
             COMMAND ${CMD} 8 0 -L=info tblmeta
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-8.1-MetadataAttrsTbl-Packed
             COMMAND ${CMD} 8packed 0 -L=info tblmeta
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set ( CMD sh runtestcase.sh ${DIRTOTEST} "${DIRTOTEST}/kdbmeta db/tbl/REFERENCE/col/PRIMARY_ALIGNMENT_IDS" ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test ( NAME GeneralLoader-9.0-MetadataAttrsCol-Unpacked
             COMMAND ${CMD} 9 0 -L=info colmeta
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME GeneralLoader-9.1-MetadataAttrsCol-Packed
             COMMAND ${CMD} 9packed 0 -L=info colmeta
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties(
        GeneralLoader-1.0-Basic-Unpacked
        GeneralLoader-1.1-Basic-Packed
        GeneralLoader-1.2-TargetDbOverride
        GeneralLoader-1.3-TargetDbOverrideShorthand
        GeneralLoader-2.0-ErrorMessageEvent-Unpacked
        GeneralLoader-2.1-ErrorMessageEvent-Packed
        GeneralLoader-3.0-EmptyDefaultValues-Unpacked
        GeneralLoader-3.1-EmptyDefaultValues-Packed
        GeneralLoader-4.0-MoveAheadEvent-Unpacked
        GeneralLoader-4.1-MoveAheadEvent-Packed
        GeneralLoader-5.0-IntegerConversion
        GeneralLoader-6.0-SoftwareName-Unpacked
        GeneralLoader-6.1-SoftwareName-Packed
        GeneralLoader-7.0-MetadataAttrsDb-Unpacked
        GeneralLoader-7.1-MetadataAttrsDb-Packed
        GeneralLoader-8.0-MetadataAttrsTbl-Unpacked
        GeneralLoader-8.1-MetadataAttrsTbl-Packed
        GeneralLoader-9.0-MetadataAttrsCol-Unpacked
        GeneralLoader-9.1-MetadataAttrsCol-Packed

        PROPERTIES FIXTURES_REQUIRED Fixture
    )

endif()

add_subdirectory(vschema)
