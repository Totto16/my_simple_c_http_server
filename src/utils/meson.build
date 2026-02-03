src_files += files(
    'clock.c',
    'clock.h',
    'errors.c',
    'errors.h',
    'log.c',
    'log.h',
    'myqueue.c',
    'myqueue.h',
    'path.c',
    'path.h',
    'sized_buffer.c',
    'sized_buffer.h',
    'parse.c',
    'parse.h',
    'string_builder.c',
    'string_builder.h',
    'string_helper.c',
    'string_helper.h',
    'thread_helper.c',
    'thread_helper.h',
    'thread_pool.c',
    'thread_pool.h',
    'utf8_helper.c',
    'utf8_helper.h',
    'utils.c',
    'utils.h',
    'tmap_common_impl.c',
)


## detect if we have the sys/sysinfo.h header

cc = meson.get_compiler('c')

has_sys_sysinfo_header = cc.check_header('sys/sysinfo.h')
if not has_sys_sysinfo_header
    compile_flags += '-D_DONT_HAVE_SYS_SYSINFO'
endif

stdc_version = cc.get_define('__STDC_VERSION__ ')
stdc_version_int = stdc_version.replace('L', '').to_int()
if stdc_version_int >= 202000
    compile_flags += '-D_SIMPLE_SERVER_COMPILE_WITH_NARROWED_ENUMS'
endif

