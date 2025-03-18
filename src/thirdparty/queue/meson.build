

## detect if we need to include this header

cc = meson.get_compiler('c')


has_sys_queue_header = cc.check_header('sys/queue.h')
if not has_sys_queue_header
    inc_dirs += include_directories('.')
    compile_flags += '-D_USE_BSD_QUEUE'
endif
