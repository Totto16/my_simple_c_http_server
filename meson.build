project(
    'simple http server',
    'c',
    license: 'MIT',
    meson_version: '>=1.2.0',
    default_options: {
        'buildtype': 'release',
        'optimization': '3',
        'c_std': 'gnu2x',
        'warning_level': '3',
        'werror': 'true',
        'b_ndebug': 'if-release',
    },
    version: '3.0.1',
)

deps = []
compile_flags = [
    '-DPROGRAM_NAME=' + meson.project_name(),
    '-DVERSION_STRING=' + meson.project_version(),
]

deps += dependency('threads')

secure_option = get_option('secure')

openssl_dep = dependency('openssl', required: secure_option)

if openssl_dep.found()
    deps += openssl_dep
else
    compile_flags += '-D_HTTP_SERVER_SECURE_DISABLED'

endif

deps += dependency(
    'libutf8proc',
    allow_fallback: true,
    default_options: {'tests': 'disabled'},
)

src_files = []
inc_dirs = []

subdir('src')

executable(
    'http',
    src_files,
    dependencies: deps,
    include_directories: inc_dirs,
    c_args: compile_flags,
)
