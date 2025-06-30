src_files += files(
    'buffer.c',
    'buffer.h',
    'common.c',
    'common.h',
    'constants.h',
    'export.h',
    'file.c',
    'file.h',
    'generated_constants.c',
    'log.h',
    'macro.h',
    'string.c',
    'lzws_string.h',
    'utils.c',
    'utils.h',
)

inc_dirs += include_directories('.')

subdir('compressor')
subdir('decompressor')
subdir('bigint')

config_h = configure_file(output: 'config.h', configuration: conf)

src_files += config_h
