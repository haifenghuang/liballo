project(
    'liballo',
    'c',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: [
        'warning_level=3',
        'c_std=c11',
        'b_lundef=false',
    ],
)

cc = meson.get_compiler('c')
liballo_headers = include_directories('include')

install_headers('include/allo.h', subdir: 'liballo')

subdir('src')
subdir('tests')

if get_option('benchmarks')
    subdir('benchmarks')
endif
