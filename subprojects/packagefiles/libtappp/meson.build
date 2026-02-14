project(
    'libtappp',
    'cpp',
    version: '1.0.0',
    meson_version: '>= 1.3.0',
    default_options: {
        'cpp_std': ['c++11'],
        'werror': 'false',
    },
)

compile_args = []
src_files = []
inc_dirs = include_directories('include')


src_files += files(
    'src/tap++.cpp'
)

libtappp = library(
    'libtappp',
    src_files,
    cpp_args: compile_args + ['-Wno-dynamic-exception-spec'],
    include_directories: inc_dirs,
    pic: true,
)

libtappp_dep = declare_dependency(
    link_with: libtappp,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('libtappp', libtappp_dep)
