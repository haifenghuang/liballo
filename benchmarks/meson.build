ubench_proj = subproject('ubench')
ubench_dep = ubench_proj.get_variable('ubench_dep')

m_dep = cc.find_library('m', required: false)

bench_allocators_exe = executable(
    'bench_allocators',
    'bench_allocators.c',
    include_directories: liballo_headers,
    link_with: liballo_lib,
    dependencies: [ubench_dep, m_dep],
)

benchmark('bench_allocators', bench_allocators_exe)
