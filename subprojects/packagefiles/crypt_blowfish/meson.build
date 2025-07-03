project(
    'crypt_blowfish',
    'c',
    version: '1.3',
    meson_version: '>= 1.3.0',
    default_options: {
        'c_std': ['gnu23', 'gnu2x', 'gnu17', 'gnu11'],
        'werror': 'false',
    },
)

compile_args = []
src_files = []
inc_dirs = include_directories('.')

private_args = [
    '-fvisibility=hidden',
    '-Wall',
    '-Wbad-function-cast',
    '-Wcast-align',
    '-Wcast-qual',
    '-Wmissing-prototypes',
    '-Wstrict-prototypes',
    '-Wshadow',
    '-Wundef',
    '-Wpointer-arith',
    '-fomit-frame-pointer',
    '-funroll-loops',
]

src_files += files('crypt.h', 'crypt_blowfish.c'
, 'crypt_blowfish.h', 'crypt_gensalt.c', 'crypt_gensalt.h', 'ow-crypt.h', 'wrapper.c')



libcrypt_blowfish = library(
    'libbcrypt',
    src_files,
    c_args: compile_args + private_args,
    include_directories: inc_dirs,
)

crypt_blowfish_dep = declare_dependency(
    link_with: libcrypt_blowfish,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('crypt_blowfish', crypt_blowfish_dep)
