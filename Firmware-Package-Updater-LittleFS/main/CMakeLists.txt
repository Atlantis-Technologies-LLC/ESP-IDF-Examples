set(app_sources
    "main.c"
    "web_server.c"
    # Add other source files here manually
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "." "../include"
    EMBED_TXTFILES ${CMAKE_SOURCE_DIR}/version_info/build_info.txt  ## Embed build_info.txt into firmware
)