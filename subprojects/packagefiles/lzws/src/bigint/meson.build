src_files += files('common.h', 'main.h')


if get_option('bignum_library') == 'gmp'
    src_files += files('gmp_int.c', 'gmp_int.h')
    conf.set('LZWS_BIGNUM_LIBRARY_GMP', 1)
    deps += dependency('gmp')
else
    src_files += files('tommath_int.c', 'tommath_int.h')
    conf.set('LZWS_BIGNUM_LIBRARY_TOMMATH', 1)
    deps += dependency('tommath')
endif
