liballo_sources = [
    'allo.c',
    'allo_buffer.c',
    'allo_arena.c',
    'allo_pool.c',
    'allo_buddy.c',
    'allo_utils.c',
    'allo_fallback.c',
]

deps = []
lib_link_args = []

if opt_freestanding
    lib_link_args += ['-nostdlib']
else
    liballo_sources += ['allo_libc.c', 'allo_mtx.c', 'allo_page.c', 'allo_gen.c']
    deps += threads_dep
endif

liballo_lib = library(
    'allo',
    liballo_sources,
    include_directories: liballo_headers,
    dependencies: deps,
    link_args: lib_link_args,
    install: true,
)

if can_do_nostdlib
    liballo_static = static_library(
        'allo_static',
        liballo_sources,
        include_directories: liballo_headers,
        c_args: ['-DALLO_FREESTANDING', '-ffreestanding'],
        install: false, # internal use only
    )
endif
