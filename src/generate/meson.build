

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

type_check_hpack_ts = run_command(
    pnpm,
    '--dir',
    meson.current_source_dir(),
    'exec',
    npx,
    'tsc',
    '-p',
    'tsconfig.json',
    check: true,
)

if type_check_hpack_ts.returncode() != 0
    error('script didn\'t typecheck')
endif

generated_hpack_file_c = custom_target(
    'generated_hpack.c',
    command: [
        pnpm,
        '--dir',
        '@CURRENT_SOURCE_DIR@',
        'exec',
       node,
        'hpack.ts',
        '--type',
        'c',
        '-o',
        '@OUTPUT@',
    ],
    output: ['generated_hpack.c'],
    depend_files: hpack_script_depend_files,
)

src_files += generated_hpack_file_c

generated_hpack_file_cpp = custom_target(
    'generated_hpack_tests.hpp',
    command: [
        pnpm,
        '--dir',
        '@CURRENT_SOURCE_DIR@',
        'exec',
        node,
        'hpack.ts',
        '--type',
        'cpp_tests',
        '-o',
        '@OUTPUT@',
    ],
    output: ['generated_hpack_tests.hpp'],
    depend_files: hpack_script_depend_files,
)


generated_cpp_hpack_tests += generated_hpack_file_cpp
