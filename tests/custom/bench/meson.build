bench_deps = []
bench_src = []
bench_inc_dirs = []

## see: https://github.com/doctest/doctest/commit/5eb20178ba43d7b3fd6dbf97b4d2a0907b59a42a
# and https://github.com/doctest/doctest/pull/901
# for the reason, why this is needed  like this

bench_deps += dependency(
    'google-benchmark',
    version: '>=1.8.0',
    fallback: ['google-benchmark', 'google_benchmark_dep'],
    default_options: {
        'tests': 'disabled',
    },
)

bench_deps += http_server_dep

bench_deps += test_support_dep

bench_inc_dirs += include_directories('.')


bench_src += files('main.cpp')

bench_src += files(
    'hpack/huffman.cpp',
    'hash.cpp',
    'parsing.cpp',
    'serialize.cpp',
    'string_compare.cpp',
)

bench_exe = executable(
    'bench',
    bench_src,
    include_directories: bench_inc_dirs,
    dependencies: bench_deps,
    cpp_args: internal_cpp_args,
    c_args: internal_c_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': cpp_std_to_use,
    },
)

benchmark(
    'bench',
    bench_exe,
    protocol: 'exitcode',
    workdir: workdir,
)
