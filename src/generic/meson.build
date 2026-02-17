src_files += files(
    'authentication.c',
    'authentication.h',
    'endian_compat.h',
    'hash.c',
    'hash.h',
    'helper.c',
    'helper.h',
    'ip.c',
    'ip.h',
    'read.c',
    'read.h',
    'secure.c',
    'secure.h',
    'sem.c',
    'sem.h',
    'send.c',
    'send.h',
    'serialize.c',
    'serialize.h',
    'signal_fd.c',
    'signal_fd.h',
)



## detect if we have the sys/signalfd.h header

cc = meson.get_compiler('c')

has_sys_signalfd_header = cc.check_header('sys/signalfd.h')
if not has_sys_signalfd_header
    compile_flags += '-D_DONT_HAVE_SYS_SIGNALFD'
endif
