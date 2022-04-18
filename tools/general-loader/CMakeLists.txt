add_executable ( general-loader
    general-loader.cpp
    protocol-parser.cpp
    database-loader.cpp
    main.cpp
)

target_include_directories ( general-loader PRIVATE ${CMAKE_SOURCE_DIR}/libs/inc )

add_dependencies ( general-loader general-writer )

target_link_libraries ( general-loader
    general-writer
    loader
    kapp
    tk-version
	${COMMON_LIBS_WRITE}
)

MakeLinksExe ( general-loader false )

add_subdirectory ( test )
