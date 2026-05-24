test_cases = [
    'test_c',
    'test_buffer',
    'test_arena',
    'test_pool',
    'test_common',
    'test_page',
    'test_realloc',
    'test_buddy',
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
    fuzz_args = []
    if not get_option('b_coverage')
        fuzz_args += ['-fsanitize=fuzzer', '-DFUZZING_DRIVER']
    endif
    fuzz_exe = executable(
        'fuzz_allocator',
        'fuzz_allocator.c',
        include_directories: liballo_headers,
        link_with: liballo_lib,
        c_args: fuzz_args,
        link_args: fuzz_args,
    )
endif

# Special nostdlib test case (Linux x86_64 only)
is_afl = cc.get_id() == 'clang' and cc.cmd_array()[0].contains('afl-clang')
if not is_afl and host_machine.cpu_family() == 'x86_64' and host_machine.system() == 'linux'
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
        override_options: ['b_sanitize=none', 'b_coverage=false'],
    )
    test('test_page_nostdlib', test_page_nostdlib_exe)
endif
