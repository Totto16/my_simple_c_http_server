src_files += files('account.c', 'account.h', 'command.c', 'command.h', 'data.c'
, 'data.h', 'file_ops.c', 'file_ops.h', 'protocol.h'
, 'send.c', 'send.h', 'server.c', 'server.h', 'state.c', 'state.h')


## detect if __sighandler_t is defined

cc = meson.get_compiler('c')


has___sighandler_t_type = cc.compiles(
    '''
#include <signal.h>

int f(__sighandler_t h);
'''
)
if not has___sighandler_t_type
    compile_flags += '-D_NO_SIGNAL_HANDLER_TYPED_DEFINED'
endif
