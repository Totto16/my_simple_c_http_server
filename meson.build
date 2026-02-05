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
    version: '3.5.0-alpha',
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

other_features_dict = {
    'bcrypt': false,
    'pam': false,
}

other_features = get_option('other_features')

foreach other_feature : other_features
    other_features_dict = (
        other_features_dict + {
                other_feature: true,
            }
    )
endforeach


libbcrypt_dep = dependency(
    'libbcrypt',
    required: other_features_dict['bcrypt'],
    allow_fallback: true,
)

if libbcrypt_dep.found()
    deps += libbcrypt_dep
    compile_flags += '-D_SIMPLE_SERVER_HAVE_BCRYPT'
endif

pam_dep = dependency(
    'pam',
    required: other_features_dict['pam'],
    allow_fallback: true,
)

if pam_dep.found()
    pam_misc_dep = dependency(
        'pam_misc',
        required: other_features_dict['pam'],
        allow_fallback: true,
    )
    if pam_misc_dep.found()
        deps += [pam_dep, pam_misc_dep]
        compile_flags += '-D_SIMPLE_SERVER_HAVE_PAM'
    endif

endif


compression_features_dict = {
    'zstd': false,
    'br': false,
    'deflate': false,
    'gzip': false,
    'compress': false,
}

compression_features = get_option('compression_features')

foreach compression_feature : compression_features
    compression_features_dict = (
        compression_features_dict + {
                compression_feature: true,
            }
    )
endforeach


zlib_dep = dependency(
    'zlib',
    required: compression_features_dict['gzip'] or compression_features_dict['deflate'],
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

deps += dependency(
    'tvec.h',
    allow_fallback: true,
)

deps += dependency(
    'tmap.h',
    allow_fallback: true,
)

cc = meson.get_compiler('c')

deps += cc.find_library(
    'm',
    required: false,
)

internal_c_args = []



internal_c_args += cc.get_supported_arguments(
    '-Wassign-enum',
    '-Wenum-conversion',
    '-Wenum-switch',
    '-fstrict-enums',
    '-Wenum-identifier',
    '-Wenum-compare',
    '-Wenum-enum-conversion',
    '-Wenum-float-conversion',
    '-Wenum-too-large',
    '-Wenum-compare-switch',
    '-Wenum-compare-conditional',
    '-fno-strict-aliasing',
    '-Wpointer-arith',
    '-Wmissing-declarations',
    '-Wformat=2',
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wundef',
    '-Wunused',
    '-Wcast-align',
    '-Wmissing-noreturn',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wlogical-op',
    '-Wignored-qualifiers',
    '-Werror=redundant-decls',
    '-Werror=implicit',
    '-Werror=nonnull',
    '-Werror=init-self',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=sequence-point',
    '-Werror=return-type',
    '-Werror=trigraphs',
    '-Werror=array-bounds',
    '-Werror=write-strings',
    '-Werror=address',
    '-Werror=int-to-pointer-cast',
    '-Werror=pointer-to-int-cast',
    '-Werror=empty-body',
    '-Werror=write-strings',
    '-Werror=strict-aliasing',
    '-Wsign-compare',
    '-Wno-cast-function-type',
    '-Wunused-parameter',
    '-Wmissing-field-initializers',
    '-Wtype-limits',
    '-Wshadow',
    '-Wfloat-conversion',
    '-Wno-nullability-extension',
    '-Wno-nullability-completeness',
)

src_files = []
inc_dirs = []

subdir('src')

http_server_lib = library(
    'http_server',
    src_files,
    dependencies: deps,
    include_directories: inc_dirs,
    c_args: [compile_flags, internal_c_args],
)

http_server_dep = declare_dependency(
    link_with: http_server_lib,
    include_directories: inc_dirs,
    compile_args: [compile_flags],
    dependencies: deps,
    version: meson.project_version(),
)

executable(
    'server',
    files('src/main.c'),
    c_args: [internal_c_args],
    dependencies: [http_server_dep],
)


if get_option('tests')
    subdir('tests')
endif
