test_deps = []
test_src = []
test_inc_dirs = []

## see: https://github.com/doctest/doctest/commit/5eb20178ba43d7b3fd6dbf97b4d2a0907b59a42a
# and https://github.com/doctest/doctest/pull/901
# for the reason, why this is needed  like this

test_deps += dependency(
    'doctest_patched',
    version: '>=2.4.12',
    fallback: ['doctest', 'doctest_dep'],
)

test_inc_dirs += include_directories('.')

test_src += files('c_types.hpp', 'hash.cpp', 'http_parser.cpp', 'main.cpp')

add_languages(
    'cpp',
    native: false,
)

cpp = meson.get_compiler('cpp')



internal_cpp_args = []

internal_cpp_args += cc.get_supported_arguments(
internal_c_args
)


tests = executable(
    'tests',
    test_src,
    include_directories: test_inc_dirs,
    dependencies: [test_deps, http_server_dep],
    cpp_args: internal_cpp_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': ['c++23', 'c++latest', 'c++20'],
    },
)

test(
    'tests',
    tests,
    protocol: 'exitcode',
    workdir: meson.project_source_root() / 'tests' / 'unit' / 'files',
)
