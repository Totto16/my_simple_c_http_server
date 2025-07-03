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
    version: '3.3.0',
)

deps = []
compile_flags = [
    '-DPROGRAM_NAME=' + meson.project_name(),
    '-DVERSION_STRING=' + meson.project_version(),
]

deps += dependency('threads')

secure_option = get_option('secure')

openssl_dep = dependency(
    'openssl',
    required: secure_option,
    version: '>=3.0',
)

if openssl_dep.found()
    deps += openssl_dep
    compile_flags += '-D_SIMPLE_SERVER_USE_OPENSSL_FOR_HASHING'
    compile_flags += '-D_SIMPLE_SERVER_USE_OPENSSL'

    cc = meson.get_compiler('c')

    has_newer_sha_functions = cc.check_header(
        'openssl/evp.h',
        dependencies: [openssl_dep],
    )
    if not has_newer_sha_functions
        compile_flags += '-D_SIMPLE_SERVER_USE_DEPRECATED_OPENSSL_SHA_FUNCTIONS'
    endif


else
    compile_flags += '-D_SIMPLE_SERVER_SECURE_DISABLED'
endif

# TODO:
warning('todo')
## find library for bcrypt instead of openssl!
# libcrypt


compression_features_dict = {
    'zstd': false,
    'br': false,
    'deflate': false,
    'gzip': false,
    'compress': false,
}

compression_features = get_option('compression_features')

foreach compression_feature : compression_features
    compression_features_dict = compression_features_dict + {
        compression_feature: true,
    }
endforeach


zlib_dep = dependency(
    'zlib',
    required: compression_features_dict['gzip'] or compression_features_dict['deflate'
],
)

if zlib_dep.found()
    deps += zlib_dep
    compile_flags += '-D_SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP'
    compile_flags += '-D_SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE'
endif

brotli_dep = dependency(
    'brotli',
    'libbrotlienc',
    required: compression_features_dict['br'],
)

if brotli_dep.found()
    deps += brotli_dep
    compile_flags += '-D_SIMPLE_SERVER_COMPRESSION_SUPPORT_BR'
endif

zstd_dep = dependency(
    'libzstd',
    required: compression_features_dict['zstd'],
)

if zstd_dep.found()
    deps += zstd_dep
    compile_flags += '-D_SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD'
endif

lzws_dep = dependency(
    'lzws',
    required: compression_features_dict['compress'],
)

if lzws_dep.found()
    deps += lzws_dep
    compile_flags += '-D_SIMPLE_SERVER_COMPRESSION_SUPPORT_COMPRESS'
endif


deps += dependency(
    'libutf8proc',
    allow_fallback: true,
    default_options: {
        'tests': 'disabled',
    },
)

deps += dependency(
    'cwalk',
    allow_fallback: true,
)

cc = meson.get_compiler('c')

deps += cc.find_library(
    'm',
    required: false,
)

src_files = []
inc_dirs = []

subdir('src')

http_server_lib = library(
    'http_server',
    src_files,
    dependencies: deps,
    include_directories: inc_dirs,
    c_args: compile_flags,
)

http_server_dep = declare_dependency(
    link_with: http_server_lib,
    include_directories: inc_dirs,
    compile_args: compile_flags,
    dependencies: deps,
    version: meson.project_version(),
)

executable(
    'server',
    files('src/main.c'),
    dependencies: [http_server_dep],
)


if get_option('tests')
    subdir('tests/unit')
endif
