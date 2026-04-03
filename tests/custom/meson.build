support_src = []

subdir('support')

support_inc = include_directories('.')

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

if cpp.get_id() == 'gcc'
    internal_cpp_args += cpp.get_supported_arguments('-Wno-missing-braces')
endif


cpp_std_to_use = ['c++23', 'c++latest', 'c++20']

test_support_lib = library(
    '__internal_test_support',
    support_src,
    dependencies: [http_server_dep, generated_hpack_tests_cpp_dep],
    include_directories: support_inc,
    c_args: internal_c_args,
    cpp_args: internal_cpp_args,
    override_options: {
        'warning_level': '3',
        'werror': true,
        'b_coverage': false,
        'cpp_std': cpp_std_to_use,
    },
)

test_support_dep = declare_dependency(
    link_with: test_support_lib,
    include_directories: support_inc,
    compile_args: [],
    dependencies: [http_server_dep],
    sources: generated_hpack_tests_cpp_generated_headers,
    version: meson.project_version(),
)


## mark as unusable
correct_cpp_args = {
    'don\'t_use': true,
}
support_inc = {
    'don\'t_use': true,
}
support_src = {
    'don\'t_use': true,
}
generated_cpp_tests = {
    'don\'t_use': true,
}

subdir('unit')
subdir('bench')

test_support_lib = {
    'don\'t_use': true,
}
test_support_dep = {
    'don\'t_use': true,
}
