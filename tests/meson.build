freestanding_test_cases = [
    'test_buffer',
    'test_arena',
    'test_pool',
    'test_common',
    'test_buddy',
    'test_fallback',
]

hosted_test_cases = freestanding_test_cases + [
    'test_page',
    'test_realloc',
    'test_calloc',
    'test_mtx',
    'test_c',
]

if not opt_freestanding
    foreach t : hosted_test_cases
        exe = executable(
            t,
            t + '.c',
            include_directories: liballo_headers,
            link_with: liballo_lib,
            dependencies: [threads_dep],
        )
        test(t, exe)
    endforeach

    executable(
        'fuzz_allocator',
        'fuzz_allocator.c',
        include_directories: liballo_headers,
        link_with: liballo_lib,
    )
endif

if can_do_nostdlib
    foreach t : freestanding_test_cases
        exe = executable(
            t + '_nostdlib',
            [t + '.c', 'nolibc_start.c'],
            include_directories: liballo_headers,
            link_with: liballo_static,
            c_args: [
                '-DALLO_FREESTANDING',
                '-DALLO_NOSTDLIB',
                '-ffreestanding',
                '-fno-stack-protector',
            ],
            link_args: ['-nostdlib', '-static'],
            override_options: ['b_sanitize=none', 'b_coverage=false'],
        )
        test(t + '_nostdlib', exe)
    endforeach
endif
