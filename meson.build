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

other_features_dict = {
    'bcrypt': false,
}

other_features = get_option('other_features')

foreach other_feature : other_features
    other_features_dict = other_features_dict + {
        other_feature: true,
    }
endforeach

bcrypt_lib_to_use = get_option('bcrypt_lib')

actual_bcrypt_lib_to_use = ''

if bcrypt_lib_to_use == 'bcrypt'

    bcrypt_dep = dependency(
        'bcrypt',
        required: other_features_dict['bcrypt'],
        allow_fallback: true,
    )

    if bcrypt_dep.found()
        deps += bcrypt_dep
        actual_bcrypt_lib_to_use = 'bcrypt'
    endif

elif bcrypt_lib_to_use == 'libbcrypt'

    libbcrypt_dep = dependency(
        'libbcrypt',
        required: other_features_dict['bcrypt'],
        allow_fallback: true,
    )

    if libbcrypt_dep.found()
        deps += libbcrypt_dep
        actual_bcrypt_lib_to_use = 'libbcrypt'
    endif
elif bcrypt_lib_to_use == 'crypt_blowfish'

    crypt_blowfish_dep = dependency(
        'crypt_blowfish',
        required: other_features_dict['bcrypt'],
        allow_fallback: true,
    )

    if crypt_blowfish_dep.found()
        deps += crypt_blowfish_dep
        actual_bcrypt_lib_to_use = 'crypt_blowfish'
    endif

elif bcrypt_lib_to_use == 'auto'

    bcrypt_check_order = ['libbcrypt', 'crypt_blowfish', 'bcrypt']

    foreach bcrypt_value : bcrypt_check_order
        auto_bcrypt_dep = dependency(
            bcrypt_value,
            required: false,
            allow_fallback: true,
        )

        if auto_bcrypt_dep.found()
            deps += auto_bcrypt_dep
            actual_bcrypt_lib_to_use = bcrypt_value
            break
        endif

    endforeach

    if actual_bcrypt_lib_to_use == '' and other_features_dict['bcrypt']
        error('need at least one bcrypt library, but failed to get one')
    endif

else
    error('Unsupported value for \'bcrypt_lib\': ' + bcrypt_lib_to_use)
endif

message('Using bcrypt lib: \'' + actual_bcrypt_lib_to_use + '\'')

if actual_bcrypt_lib_to_use != ''
    compile_flags += '-D_SIMPLE_SERVER_HAVE_BCRYPT_LIB'
    compile_flags += ('-D_SIMPLE_SERVER_USE_BCRYPT_LIB_' + actual_bcrypt_lib_to_use.to_upper())
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
