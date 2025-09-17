add_compile_definitions(HAVE_NANOSLEEP HAVE_REALLOCARRAY)

set(SRC
    clear.c
    compare.c
    copy.c
    gen_uuid.c
    isnull.c
    pack.c
    parse.c
    randutils.c
    unpack.c
    unparse.c
    uuid_time.c
)

add_library(extuuid STATIC ${SRC})

set_property(TARGET extuuid PROPERTY C_STANDARD 11)

install(TARGETS extuuid
    RUNTIME DESTINATION lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    )
install(FILES uuid.h uuidd.h uuidP.h
    DESTINATION include/uuid
    )
