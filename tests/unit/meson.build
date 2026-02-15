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

test_deps += dependency('nlohmann_json')

test_inc_dirs += include_directories('.')

test_src += files(
    'c_types.cpp',
    'c_types.hpp',
    'cpp_types.cpp',
    'cpp_types.hpp',
    'hpack/helpers.hpp',
    'main.cpp',
)

test_src += generated_cpp_hpack_tests

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

internal_cpp_args += [
    '-DDOCTEST_CONFIG_REQUIRE_STRINGIFICATION_FOR_ALL_USED_TYPES',
    '-DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING',
    '-DDOCTEST_CONFIG_USE_STD_HEADERS',
]

test_files = [
    'hash.cpp',
    'http_parser.cpp',
    'serialize.cpp',
    'hpack/huffman.cpp',
    'hpack/go-hpack.cpp',
    'hpack/haskell-http2-linear.cpp',
    'hpack/haskell-http2-linear-huffman.cpp',
    'hpack/haskell-http2-naive.cpp',
    'hpack/haskell-http2-naive-huffman.cpp',
    'hpack/haskell-http2-static.cpp',
    'hpack/haskell-http2-static-huffman.cpp',
    'hpack/nghttp2.cpp',
    'hpack/nghttp2-16384-4096.cpp',
    'hpack/nghttp2-change-table-size.cpp',
    'hpack/node-http2-hpack.cpp',
    'hpack/python-hpack.cpp',
    'hpack/swift-nio-hpack-huffman.cpp',
    'hpack/swift-nio-hpack-plain-text.cpp',
    'hpack/manual.cpp',
]


workdir = meson.project_source_root() / 'tests' / 'unit' / 'files'

foreach test_file : test_files

    test_name = test_file.replace('.cpp', '').replace('/', '_')

    this_test = executable(
        test_name,
        test_src,
        files(test_file),
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
        test_name,
        this_test,
        protocol: 'exitcode',
        workdir: workdir,
        is_parallel: true,
    )

endforeach


all_tests = executable(
    'all_tests',
    test_src,
    files(test_files),
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
