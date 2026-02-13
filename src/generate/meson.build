

node = find_program(
    'node',
    required: true,
)

npx = find_program(
    'npx',
    required: true,
)

pnpm = find_program(
    'pnpm',
    required: true,
)

hpack_script_depend_files = files('hpack.ts', 'package.json', 'tsconfig.json')

type_check_hpack_ts = custom_target(
    'hpack.ts',
    command: [
        pnpm,
        '--dir',
        meson.current_source_dir(),
        'exec',
        npx,
        'tsc',
        '-p',
        'tsconfig.json',
    ],
    output: ['nothing.txt'],
    depend_files: hpack_script_depend_files,
)

generated_hpack_huffman_file_c = custom_target(
    'generated_hpack_huffman.c',
    command: [
        node,
        '@CURRENT_SOURCE_DIR@/hpack.ts',
        '--type',
        'c_hpack_huffman',
        '-o',
        '@OUTPUT@',
    ],
    output: ['generated_hpack_huffman.c'],
    depend_files: hpack_script_depend_files,
    depends: [type_check_hpack_ts],
)

src_files += generated_hpack_huffman_file_c

generated_hpack_header_h = custom_target(
    'generated_hpack.h',
    command: [
        node,
        '@CURRENT_SOURCE_DIR@/hpack.ts',
        '--type',
        'c_header_table',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_hpack.h', 'generated_hpack.c'],
    depend_files: hpack_script_depend_files,
    depends: [type_check_hpack_ts],
)


src_files += generated_hpack_header_h

generated_hpack_file_cpp = custom_target(
    'generated_hpack_tests.hpp',
    command: [
        node,
        '@CURRENT_SOURCE_DIR@/hpack.ts',
        '--type',
        'cpp_tests',
        '-o',
        '@OUTPUT@',
    ],
    output: ['generated_hpack_tests.hpp'],
    depend_files: hpack_script_depend_files,
    depends: [type_check_hpack_ts],
)


generated_cpp_hpack_tests += generated_hpack_file_cpp
