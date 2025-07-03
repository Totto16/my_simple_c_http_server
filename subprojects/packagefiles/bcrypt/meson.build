project(
    'bcrypt',
    'c',
    version: '1.0.0',
    meson_version: '>= 1.5.0',
    default_options: {
        'c_std': ['gnu23', 'gnu2x', 'gnu17', 'gnu11'],
        'werror': 'false',
        'pic': 'true',
    },
)

if host_machine.system() == 'darwin'
    error('MacOS not supported')
endif

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

cc = meson.get_compiler('c')

cc.compiles(
    '''
#include <unistd.h>

int test(void){
    return getentropy(NULL, 0);
}
''',
    args: compile_args + private_args,
    include_directories: inc_dirs,
    name: 'supports getentropy()',
    required: true,
)

libbcrypt = library(
    'bcrypt',
    src_files,
    c_args: compile_args + private_args,
    include_directories: inc_dirs,
)

bcrypt_dep = declare_dependency(
    link_with: libbcrypt,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('bcrypt', bcrypt_dep)
