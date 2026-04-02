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

test_deps += http_server_dep

test_deps += support_dep

test_inc_dirs += include_directories('.')

test_src += files(
    'helpers/json.cpp',
    'helpers/json.hpp',
    'helpers/main.cpp',
    'helpers/string_maker.hpp',
)

test_src += generated_cpp_tests


test_cpp_args = [
    '-DDOCTEST_CONFIG_REQUIRE_STRINGIFICATION_FOR_ALL_USED_TYPES',
    '-DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING',
    '-DDOCTEST_CONFIG_USE_STD_HEADERS'
]

test_cpp_args += internal_cpp_args


test_files_manual = [
    'basic.cpp',
    'hash.cpp',
    'http_parser.cpp',
    'serialize.cpp',
    # hpack
    'hpack/huffman.cpp',
    'hpack/manual.cpp',
]

test_files_hpack = [
    # thirdparty hpack
    'hpack/thirdparty/go-hpack.cpp',
    'hpack/thirdparty/haskell-http2-linear.cpp',
    'hpack/thirdparty/haskell-http2-linear-huffman.cpp',
    'hpack/thirdparty/haskell-http2-naive.cpp',
    'hpack/thirdparty/haskell-http2-naive-huffman.cpp',
    'hpack/thirdparty/haskell-http2-static.cpp',
    'hpack/thirdparty/haskell-http2-static-huffman.cpp',
    'hpack/thirdparty/nghttp2.cpp',
    'hpack/thirdparty/nghttp2-16384-4096.cpp',
    'hpack/thirdparty/nghttp2-change-table-size.cpp',
    'hpack/thirdparty/node-http2-hpack.cpp',
    'hpack/thirdparty/python-hpack.cpp',
    'hpack/thirdparty/swift-nio-hpack-huffman.cpp',
    'hpack/thirdparty/swift-nio-hpack-plain-text.cpp',
]


workdir = meson.project_source_root() / 'tests' / 'unit' / 'files'

foreach test_file : test_files_manual + test_files_hpack

    test_name = test_file.replace('.cpp', '').replace('/', '_')

    this_test = executable(
        test_name,
        test_src,
        files(test_file),
        include_directories: test_inc_dirs,
        dependencies: test_deps,
        cpp_args: test_cpp_args,
        c_args: internal_c_args,
        override_options: {
            'warning_level': '3',
            'werror': true,
            'b_coverage': false,
            'cpp_std': cpp_std_to_use,
        },
    )

    test(
        test_name,
        this_test,
        protocol: 'exitcode',
        workdir: workdir,
        is_parallel: true,
        timeout: 30,
    )

endforeach

all_tests_manual = executable(
    'all_tests_manual',
    test_src,
    files(test_files_manual),
    include_directories: test_inc_dirs,
    dependencies: test_deps,
    cpp_args: test_cpp_args,
    c_args: internal_c_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': cpp_std_to_use,
    },
)

all_tests_hpack = executable(
    'all_tests_hpack',
    test_src,
    files(test_files_hpack),
    include_directories: test_inc_dirs,
    dependencies: test_deps,
    cpp_args: test_cpp_args,
    c_args: internal_c_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': cpp_std_to_use,
    },
)

all_tests = executable(
    'all_tests',
    test_src,
    files(test_files_manual + test_files_hpack),
    include_directories: test_inc_dirs,
    dependencies: test_deps,
    cpp_args: test_cpp_args,
    c_args: internal_c_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': cpp_std_to_use,
    },
)
