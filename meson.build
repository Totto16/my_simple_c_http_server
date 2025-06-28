project(
    'simple http server',
    'c',
    license: 'MIT',
    meson_version: '>=1.3.0',
    default_options: {
        'buildtype': 'release',
        'optimization': '3',
        'c_std': ['gnu23', 'gnu2x'],
        'warning_level': '3',
        'werror': 'true',
        'b_ndebug': 'if-release',
    },
    version: '3.2.1',
)

deps = []
compile_flags = [
    '-DPROGRAM_NAME=' + meson.project_name(),
    '-DVERSION_STRING=' + meson.project_version(),
]

deps += dependency('threads')

secure_option = get_option('secure')

openssl_dep = dependency('openssl', required: secure_option, version: '>=3.0')

if openssl_dep.found()
    deps += openssl_dep
else
    compile_flags += '-D_SIMPLE_SERVER_SECURE_DISABLED'
endif

zlib_dep = dependency('zlib', required: false)

if zlib_dep.found()
    deps += zlib_dep
    compile_flags += '-D_SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP'
endif


deps += dependency(
    'libutf8proc',
    allow_fallback: true,
    default_options: {'tests': 'disabled'},
)

deps += dependency('cwalk', allow_fallback: true)

cc = meson.get_compiler('c')

deps += cc.find_library('m', required: false)

src_files = []
inc_dirs = []

subdir('src')

executable(
    'server',
    src_files,
    dependencies: deps,
    include_directories: inc_dirs,
    c_args: compile_flags,
)
