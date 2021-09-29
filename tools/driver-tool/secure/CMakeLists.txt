include_directories( ../inc )

set( SRC
	busy
	payload
	base64
	except
	string
	atoi
	memset_s
)

GenerateStaticLibs( secure "${SRC}" )
target_compile_definitions(secure PUBLIC "UTF8PROC_STATIC")
