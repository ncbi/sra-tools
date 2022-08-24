if ( NOT WIN32)

    #TODO: simplify
    set ( CMD ${CMAKE_CURRENT_SOURCE_DIR}/runtestcase.sh
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/general-loader
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vdb-dump ${CMAKE_CURRENT_SOURCE_DIR} )

    #
    # bio-end tests
    #
    add_executable ( bio-end-makeinputs bio-end/makeinputs )

    add_dependencies ( bio-end-makeinputs general-writer )

    target_link_libraries ( bio-end-makeinputs
        general-writer
    )

    set ( CMD ${CMAKE_CURRENT_SOURCE_DIR}/runtestcase.sh
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/general-loader ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vdb-dump ${CMAKE_CURRENT_SOURCE_DIR} )

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bio-end/input)
    add_test ( NAME bio-end-makeinputs
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bio-end-makeinputs
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test ( NAME BioEnd-Excl-1
            COMMAND ${CMD} bio-end excl-1 0 n -I${VDB_INCDIR} -L=debug
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME BioEnd-Excl-2
            COMMAND ${CMD} bio-end excl-2 0 n -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME BioEnd-Excl-3
            COMMAND ${CMD} bio-end excl-3 0 n -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME BioEnd-Excl-4
            COMMAND ${CMD} bio-end excl-4 0 n -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME BioEnd-Incl-1
            COMMAND ${CMD} bio-end incl-1 0 n -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test ( NAME IdxText-CI-1
            COMMAND ${CMD} bio-end excl-1 0 n -I${VDB_INCDIR} # -L=debug
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties(
        BioEnd-Excl-1
        BioEnd-Excl-2
        BioEnd-Excl-3
        BioEnd-Excl-4
        BioEnd-Incl-1
        PROPERTIES DEPENDS bio-end-makeinputs
    )

    #
    # idx-text tests
    #
    add_executable ( idx-text-makeinputs idx-text/makeinputs )

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/idx-text/input)
    add_dependencies ( idx-text-makeinputs general-writer )
    target_link_libraries ( idx-text-makeinputs
        general-writer
    )

    add_executable ( idx-text-checklookup idx-text/checklookup )
    target_link_libraries ( idx-text-checklookup
        ${COMMON_LIBS_READ}
    )

    add_test ( NAME idx-text-makeinputs
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-makeinputs
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test ( NAME IdxText-ci-1
            COMMAND ${CMD} idx-text ci-1 0 y ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-checklookup -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME IdxText-ci-2
            COMMAND ${CMD} idx-text ci-2 0 y ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-checklookup -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME IdxText-ci-3
            COMMAND ${CMD} idx-text ci-3 0 y ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-checklookup -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME IdxText-ci-4
            COMMAND ${CMD} idx-text ci-4 0 y ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-checklookup -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME IdxText-ci-5
            COMMAND ${CMD} idx-text ci-5 0 n -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test ( NAME IdxText-cs-1
            COMMAND ${CMD} idx-text cs-1 0 y ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-checklookup -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME IdxText-cs-2
            COMMAND ${CMD} idx-text cs-2 0 y ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/idx-text-checklookup -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test ( NAME IdxText-cs-3
            COMMAND ${CMD} idx-text cs-3 0 n  -I${VDB_INCDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_tests_properties(
        IdxText-ci-1
        IdxText-ci-2
        IdxText-ci-3
        IdxText-ci-4
        IdxText-ci-5
        IdxText-cs-1
        IdxText-cs-2
        IdxText-cs-3
        PROPERTIES
            DEPENDS bio-end-makeinputs
            DEPENDS bio-end-checklookup
    )

endif()
