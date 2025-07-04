project(
    'libbcrypt',
    'c',
    version: '1.0.0',
    meson_version: '>= 1.3.0',
    default_options: {
        'c_std': ['gnu23', 'gnu2x', 'gnu17', 'gnu11'],
        'werror': 'false',
        'pic': 'true',
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

src_files += files(
    'base64.c',
    'base64.h',
    'bcrypt.c',
    'bcrypt.h',
    'keccak.c',
    'keccak.h',
    'sha512.c',
    'sha512.h',
)

subdir('crypt_blowfish')

libbcrypt = library(
    'libbcrypt',
    src_files,
    c_args: compile_args + private_args,
    include_directories: inc_dirs,
)

libbcrypt_dep = declare_dependency(
    link_with: libbcrypt,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('libbcrypt', libbcrypt_dep)
