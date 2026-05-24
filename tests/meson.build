test_cases = [
    'test_c',
    'test_buffer',
    'test_arena',
    'test_pool',
    'test_common',
    'test_page',
    'test_realloc',
]

# Standard tests
foreach t : test_cases
    exe = executable(
        t,
        t + '.c',
        include_directories: liballo_headers,
        link_with: liballo_lib,
    )
    test(t, exe)
endforeach

# Fuzzer (Clang only)
cc = meson.get_compiler('c')
if cc.get_id() == 'clang'
    fuzz_exe = executable(
        'fuzz_allocator',
        'fuzz_allocator.c',
        include_directories: liballo_headers,
        link_with: liballo_lib,
        c_args: ['-fsanitize=fuzzer'],
        link_args: ['-fsanitize=fuzzer'],
    )
endif

# Special nostdlib test case (Linux x86_64 only)
if host_machine.cpu_family() == 'x86_64' and host_machine.system() == 'linux'
    test_page_nostdlib_exe = executable(
        'test_page_nostdlib',
        [
            'test_page_nostdlib.c',
            '../src/allo_page.c',
            '../src/allo_utils.c',
        ],
        include_directories: liballo_headers,
        c_args: ['-DALLO_NOSTDLIB', '-ffreestanding', '-fno-stack-protector'],
        link_args: ['-nostdlib', '-static'],
        override_options: ['b_sanitize=none'],
    )
    test('test_page_nostdlib', test_page_nostdlib_exe)
endif
