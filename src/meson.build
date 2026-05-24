liballo_sources = [
    'allo_libc.c',
    'allo_buffer.c',
    'allo_arena.c',
    'allo_pool.c',
    'allo_page.c',
    'allo_buddy.c',
    'allo_utils.c',
]

liballo_lib = library(
    'allo',
    liballo_sources,
    include_directories: liballo_headers,
    install: true,
)
