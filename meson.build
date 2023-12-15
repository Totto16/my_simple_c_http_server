project(
    'simpel http server',
    'c',
    license: 'MIT',
    meson_version: '>=1·2.0',
    default_options: {
        'buildtype': 'release',
        'optimization': '3',
        'c_std': 'gnu2x',
        'warning_level': '3',
        'werror': 'true',
        'b_ndebug': 'if-release',
    },
    version: '2.1.0',
)


deps = []

deps += dependency('threads')

src_files = []
header_files = []
inc_dirs = []

compile_flags = [
    '-DPROGRAM_NAME=' + meson.project_name(),
    '-DVERSION_STRING=' + meson.project_version(),
]


subdir('src')

executable(
    'http',
    src_files,
    header_files,
    dependencies: deps,
    include_directories: inc_dirs,
    c_args: compile_flags,
)
