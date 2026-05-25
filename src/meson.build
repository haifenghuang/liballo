liballo_sources = [
    'allo.c',
    'allo_buffer.c',
    'allo_arena.c',
    'allo_pool.c',
    'allo_page.c',
    'allo_buddy.c',
    'allo_utils.c',
    'allo_fallback.c',
    'allo_gen.c',
]

deps = []

if not get_option('freestanding')
    liballo_sources += ['allo_libc.c', 'allo_mtx.c']
    deps += [threads_dep]
endif

lib_link_args = []
if get_option('freestanding')
    lib_link_args += ['-nostdlib']
endif

liballo_lib = library(
    'allo',
    liballo_sources,
    include_directories: liballo_headers,
    dependencies: deps,
    link_args: lib_link_args,
    install: true,
)
