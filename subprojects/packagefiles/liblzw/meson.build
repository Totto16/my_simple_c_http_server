project(
    'liblzw',
    'c',
    version: '0.3',
    meson_version: '>= 1.3.0',
    default_options: {'c_std': ['gnu11'], 'werror': 'false'},
)

conf = configuration_data()

conf.set_quoted('VERSION', meson.project_version())

cc = meson.get_compiler('c')

headers_to_check = [
    'stdint.h',
    'inttypes.h',
    'sys/types.h',
    'sys/stat.h',
    'ctype.h',
    'errno.h',
    'unistd.h',
    'fcntl.h',
    'stdio.h',
    'stdlib.h',
    'time.h',
    'string.h',
    'features.h',
    'stdarg.h',
    'sys/cdefs.h',
    'assert.h',
]

foreach header_to_check : headers_to_check
    if cc.has_header(header_to_check)
        NAME = header_to_check.replace('/', '_').replace('.', '_').to_upper()
        conf.set('HAVE_' + NAME, 1)
    endif
endforeach

config_h = configure_file(output: 'config.h', configuration: conf)

compile_args = []

if cc.get_id() == 'clang'
    compile_args += ['-Wno-variadic-macros','-Wno-gnu-pointer-arith']
endif

liblzw = library('http_server', files('lzw.c'), config_h, c_args:compile_args)

liblzw_dep = declare_dependency(
    link_with: liblzw,
    include_directories: include_directories('.'),
    version: meson.project_version(),
)
