


npx = find_program(
    'npx',
    required: false,
)

pnpm = find_program(
    'pnpm',
    required: false,
)


hpack_script_depend_files = files('hpack.ts', 'package.json', 'tsconfig.json')

hpack_script_depends = []

if npx.found() and pnpm.found()
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
    hpack_script_depends += type_check_hpack_ts
endif

node = find_program(
    'node',
    required: false,
    version: '>=24.0.0',  # for .ts execution
)

deno = find_program(
    'deno',
    required: false,
)

bun = find_program(
    'bun',
    required: false,
)

node_like_runtime = disabler()

if node.found()
    node_like_runtime = node
elif deno.found()
    node_like_runtime = deno
elif bun.found()
    node_like_runtime = bun
else
    error('No JS runtime to geneate sources')
endif

generated_hpack_huffman_file_c = custom_target(
    'generated_hpack_huffman.c',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@/hpack.ts',
        '--type',
        'c_hpack_huffman',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_hpack_huffman.h', 'generated_hpack_huffman.c'],
    depend_files: hpack_script_depend_files,
    depends: hpack_script_depends,
)

src_files += generated_hpack_huffman_file_c

generated_hpack_header_h = custom_target(
    'generated_hpack.h',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@/hpack.ts',
        '--type',
        'c_header_table',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_hpack.h', 'generated_hpack.c'],
    depend_files: hpack_script_depend_files,
    depends: hpack_script_depends,
)


src_files += generated_hpack_header_h

generated_hpack_file_cpp = custom_target(
    'generated_hpack_tests.hpp',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@/hpack.ts',
        '--type',
        'cpp_tests',
        '-o',
        '@OUTPUT@',
    ],
    output: ['generated_hpack_tests.hpp'],
    depend_files: hpack_script_depend_files,
    depends: hpack_script_depends,
)


generated_cpp_hpack_tests += generated_hpack_file_cpp
