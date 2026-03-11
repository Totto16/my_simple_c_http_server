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
)

bench_inc_dirs += include_directories('.')

bench_src += files('main.cpp')

bench_src += generated_cpp_hpack_tests

add_languages(
    'cpp',
    native: false,
)

correct_cpp_args = []

foreach internal_c_arg : internal_c_args

    # cpp.get_supported_arguments reports true, but it is not really
    if internal_c_arg not in ['-Werror=implicit', '-Werror=pointer-to-int-cast']
        correct_cpp_args += internal_c_arg
    endif

endforeach

cpp = meson.get_compiler('cpp')

internal_cpp_args = []

internal_cpp_args += cpp.get_supported_arguments(correct_cpp_args)

internal_cpp_args += []

if cpp.get_id() == 'gcc'
    internal_cpp_args += cc.get_supported_arguments('-Wno-missing-braces')
endif

bench_files = files('string_compare.cpp')

bench_exe = executable(
    'bench',
    bench_src,
    bench_files,
    include_directories: bench_inc_dirs,
    dependencies: [bench_deps, http_server_dep],
    cpp_args: internal_cpp_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': ['c++23', 'c++latest', 'c++20'],
    },
)

benchmark(
    'bench',
    bench_exe,
    protocol: 'exitcode',
    workdir: workdir
)
