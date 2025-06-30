project(
    'lzws',
    'c',
    version: '1.5.6',
    meson_version: '>= 1.3.0',
    default_options: {'c_std': ['gnu17', 'gnu11', 'c11'], 'werror': 'false'},
)


compile_args = []
src_files = []
inc_dirs = []
deps = []

conf = configuration_data()

conf.set_quoted('LZWS_VERSION', meson.project_version())

conf.set('LZWS_EXPORT_MODE_VISIBILITY', 1)

subdir('src')

private_args = ['-Dlzws_EXPORTS', '-DLZWS_EXPORT_LIBRARY_FLAG']

liblzws = library(
    'lzws',
    src_files,
    dependencies: deps,
    c_args: compile_args + private_args,
    include_directories: inc_dirs,
)

lzws_dep = declare_dependency(
    link_with: liblzws,
    dependencies: deps,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('lzws', lzws_dep)
