if get_option('compression_method') == 'linked-list'
    subdir('linked-list')
    conf.set('LZWS_COMPRESSOR_DICTIONARY_LINKED_LIST', 1)
else
    subdir('sparse-array')
    conf.set('LZWS_COMPRESSOR_DICTIONARY_SPARSE_ARRAY', 1)
endif


src_files += files('common.h', 'wrapper.h')
